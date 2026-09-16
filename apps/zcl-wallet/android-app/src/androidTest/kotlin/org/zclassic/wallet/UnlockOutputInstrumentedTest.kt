// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import java.io.File
import java.nio.file.Files
import java.nio.file.StandardOpenOption
import java.util.concurrent.ConcurrentLinkedQueue
import java.util.concurrent.CountDownLatch
import java.util.concurrent.Executor
import java.util.concurrent.ThreadPoolExecutor
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicReference
import javax.crypto.AEADBadTagException
import javax.crypto.Cipher
import javax.crypto.spec.GCMParameterSpec
import javax.crypto.spec.SecretKeySpec
import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith
import org.zclassic.wallet.core.CoreStatus
import org.zclassic.wallet.core.Network
import org.zclassic.wallet.core.TransparentAddress
import org.zclassic.wallet.core.WalletRecord
import org.zclassic.wallet.core.WalletStorage

/** Fixed public entropy, AES key and IV through real GCM and the actual unlock
 * worker. Synthetic provider faults never enter the installed Keystore. */
@RunWith(AndroidJUnit4::class)
class UnlockOutputInstrumentedTest {
    private class Fixture(size: Int = 32, private val persisted: Boolean = false) : AutoCloseable {
        private val context = ApplicationProvider.getApplicationContext<Context>()
        private val parent = Files.createTempDirectory(context.cacheDir.toPath(), "public-unlock-output-").toFile()
        private val directory = File(parent, "fixture")
        val storage = WalletStorage(directory.absolutePath)
        private val callbacks = ConcurrentLinkedQueue<Runnable>()
        val calls = SetupObservedCipher()
        private val erasedAtDispatch = ConcurrentLinkedQueue<Boolean>()
        val session = WalletPlatformSession(context, storage, Executor {
            calls.output?.let { output -> erasedAtDispatch.add(output.all { byte -> byte == 0.toByte() }) }
            callbacks.add(it)
        })
        val entropy = ByteArray(size) { 0x61 }
        private val header = WalletRecord.createHeader(entropy, Network.TESTNET)
        val expected = WalletRecord.recoveredAddress(header, entropy, Network.TESTNET)
        val prepared: PreparedWalletAction
        private val worker = WalletPlatformSession::class.java.getDeclaredField("work").apply { isAccessible = true }
            .get(session) as OwnedExecutor
        private val fatal = AtomicReference<Throwable?>()
        private val failedThread = CountDownLatch(1)
        var failure: WalletProblem? = null
        var address: TransparentAddress? = null

        init {
            val key = SecretKeySpec(ByteArray(32), "AES")
            val parameters = GCMParameterSpec(128, ByteArray(12))
            val encryption = Cipher.getInstance("AES/GCM/NoPadding")
            encryption.init(Cipher.ENCRYPT_MODE, key, parameters)
            encryption.updateAAD(header)
            val encoded = WalletRecord.pack(header, parameters.iv, encryption.doFinal(entropy))
            val decryption = calls.create()
            decryption.init(Cipher.DECRYPT_MODE, key, parameters)
            prepared = PreparedWalletAction(WalletAction.UNLOCK, decryption, Network.TESTNET,
                WalletRecord.parse(encoded), encoded)
            if (persisted) assertEquals(CoreStatus.OK, storage.create(encoded))
            val ready = CountDownLatch(1)
            assertTrue(worker.submit {
                // Only this fixture's worker catches its injected fatal error;
                // the app's global uncaught-exception behavior is unchanged.
                Thread.currentThread().uncaughtExceptionHandler = Thread.UncaughtExceptionHandler { _, problem ->
                    fatal.set(problem)
                    failedThread.countDown()
                }
                ready.countDown()
            })
            assertTrue(ready.await(10, TimeUnit.SECONDS))
        }

        fun startUnlock() {
            session.unlockAfterAuthentication(prepared, { address = it }, { failure = it })
        }

        fun unlock(expectedFatal: Throwable? = null) {
            val ciphertext = checkNotNull(prepared.record).ciphertext
            val before = ciphertext.copyOf()
            startUnlock()
            awaitIdle(expectedFatal)
            assertArrayEquals(before, ciphertext)
        }

        fun awaitIdle(expectedFatal: Throwable? = null) {
            val idle = CountDownLatch(1)
            assertTrue(worker.submit { idle.countDown() })
            assertTrue(idle.await(10, TimeUnit.SECONDS))
            if (expectedFatal != null) assertTrue(failedThread.await(5, TimeUnit.SECONDS))
            assertSame(expectedFatal, fatal.get())
            while (true) (callbacks.poll() ?: break).run()
        }

        fun assertErased() {
            val output = checkNotNull(calls.output)
            assertEquals(entropy.size, output.size)
            assertTrue("Unlock retained plaintext", output.all { it == 0.toByte() })
            assertTrue(entropy.all { it == 0x61.toByte() })
        }

        fun assertErasedBeforeDispatch() {
            assertEquals("Decrypted entropy survived into the UI handoff", listOf(true), erasedAtDispatch.toList())
        }

        fun assertNoPersistence() {
            assertNull(address)
            assertFalse("Refused unlock touched storage", directory.exists())
        }

        fun makePending() {
            check(persisted)
            Files.move(File(directory, "wallet.zcl").toPath(), File(directory, ".wallet.pending").toPath())
            assertPending()
        }

        fun assertPending() {
            val stored = storage.read()
            assertEquals(CoreStatus.OK, stored.status)
            assertTrue("Closed unlock promoted a pending wallet", stored.pending)
            assertArrayEquals(prepared.encodedRecord, stored.record)
            assertFalse(File(directory, "wallet.zcl").exists())
        }

        fun assertCommitted() {
            val stored = storage.read()
            assertEquals(CoreStatus.OK, stored.status)
            assertFalse(stored.pending)
            assertArrayEquals(prepared.encodedRecord, stored.record)
            assertFalse(File(directory, ".wallet.pending").exists())
        }

        fun replaceStoredRecord(pending: Boolean): ByteArray {
            check(persisted)
            val file = File(directory, if (pending) ".wallet.pending" else "wallet.zcl")
            assertTrue(file.isFile)
            val original = checkNotNull(prepared.encodedRecord)
            assertArrayEquals(original, file.readBytes())
            val replacement = original.copyOf()
            replacement[replacement.lastIndex] = (replacement.last().toInt() xor 1).toByte()
            // Structurally valid ciphertext, different from the authenticated
            // snapshot. The fixture never authenticates or accepts these bytes.
            WalletRecord.parse(replacement)
            Files.write(file.toPath(), replacement, StandardOpenOption.WRITE, StandardOpenOption.TRUNCATE_EXISTING)
            assertArrayEquals(replacement, file.readBytes())
            return replacement
        }

        fun assertReplacement(pending: Boolean, replacement: ByteArray) {
            val stored = storage.read()
            assertEquals(CoreStatus.OK, stored.status)
            assertEquals(pending, stored.pending)
            assertArrayEquals(replacement, stored.record)
            assertFalse(File(directory, if (pending) "wallet.zcl" else ".wallet.pending").exists())
        }

        fun awaitRetired() {
            val backend = OwnedExecutor::class.java.getDeclaredField("executor").apply { isAccessible = true }
                .get(worker) as ThreadPoolExecutor?
            if (backend != null) assertTrue(backend.awaitTermination(5, TimeUnit.SECONDS))
            while (true) (callbacks.poll() ?: break).run()
        }

        override fun close() {
            session.close()
            awaitRetired()
            entropy.fill(0)
            calls.output?.fill(0)
            if (persisted) {
                for (name in listOf("wallet.zcl", ".wallet.pending", ".lock")) {
                    val file = File(directory, name)
                    if (file.exists()) assertTrue(file.delete())
                }
                if (directory.exists()) assertTrue(directory.delete())
            }
            assertTrue("Public unlock fixture retained unexpected files", parent.delete())
        }
    }

    @Test fun providerFailureAfterDecryptErasesPlaintextBeforeReporting() {
        Fixture().use { fixture ->
            fixture.calls.afterFinal = {
                assertArrayEquals(fixture.entropy, fixture.calls.output)
                throw AEADBadTagException("Public injected post-write failure")
            }
            fixture.unlock()
            assertEquals(WalletProblem.OPERATION, fixture.failure)
            fixture.assertNoPersistence()
            fixture.assertErased()
        }
    }

    @Test fun fatalProviderFailureErasesPlaintextBeforePropagating() {
        Fixture().use { fixture ->
            val problem = OutOfMemoryError("Public injected post-write failure")
            fixture.calls.afterFinal = {
                assertArrayEquals(fixture.entropy, fixture.calls.output)
                throw problem
            }
            fixture.unlock(problem)
            assertNull(fixture.failure)
            fixture.assertNoPersistence()
            fixture.assertErased()
        }
    }

    @Test fun completeUnlockPreservesEverySupportedEntropyLengthAndErasesOutput() {
        for (size in listOf(16, 20, 24, 28, 32)) Fixture(size, persisted = true).use { fixture ->
            fixture.unlock()
            assertNull(fixture.failure)
            assertEquals(fixture.expected, fixture.address)
            assertEquals(CoreStatus.OK, fixture.storage.read().status)
            fixture.assertErased()
            fixture.assertErasedBeforeDispatch()
        }
    }

    @Test fun corruptedCiphertextRefusesWithoutCreatingStorage() {
        Fixture().use { fixture ->
            val ciphertext = checkNotNull(fixture.prepared.record).ciphertext
            ciphertext[ciphertext.lastIndex] = (ciphertext.last().toInt() xor 1).toByte()
            fixture.unlock()
            assertEquals(WalletProblem.OPERATION, fixture.failure)
            fixture.assertNoPersistence()
            // The caller-owned destination exists even if GCM refuses early.
            fixture.assertErased()
        }
    }

    @Test fun incorrectProviderLengthsRefuseAndEraseTheWholeDestination() {
        for (length in listOf(Int.MIN_VALUE, -1, 0, 31, 33, Int.MAX_VALUE)) Fixture().use { fixture ->
            fixture.calls.reportedLength = length
            fixture.unlock()
            assertEquals(WalletProblem.OPERATION, fixture.failure)
            fixture.assertNoPersistence()
            fixture.assertErased()
        }
    }

    @Test fun closingDuringProviderWorkRetainsOwnershipUntilFailureCleanup() {
        Fixture().use { fixture ->
            val entered = CountDownLatch(1)
            val release = CountDownLatch(1)
            fixture.calls.afterFinal = {
                entered.countDown()
                check(release.await(10, TimeUnit.SECONDS))
                throw AEADBadTagException("Public injected delayed provider failure")
            }
            try {
                fixture.startUnlock()
                assertTrue(entered.await(5, TimeUnit.SECONDS))
                fixture.session.close()
                // close() cannot clear a buffer while the provider still owns
                // its write. The active worker must perform that cleanup.
                assertArrayEquals(fixture.entropy, fixture.calls.output)
            } finally { release.countDown() }
            fixture.awaitRetired()
            assertNull(fixture.failure) // Retired callbacks are inert.
            fixture.assertNoPersistence()
            fixture.assertErased()
        }
    }

    @Test fun closingDuringSuccessfulDecryptionPreservesThePendingRecord() {
        Fixture(persisted = true).use { fixture ->
            fixture.makePending()
            val entered = CountDownLatch(1)
            val release = CountDownLatch(1)
            fixture.calls.afterFinal = {
                entered.countDown()
                check(release.await(10, TimeUnit.SECONDS))
            }
            try {
                fixture.startUnlock()
                assertTrue(entered.await(5, TimeUnit.SECONDS))
                InstrumentationRegistry.getInstrumentation().runOnMainSync { fixture.session.close() }
                assertArrayEquals(fixture.entropy, fixture.calls.output)
            } finally { release.countDown() }
            fixture.awaitRetired()
            assertNull(fixture.failure)
            assertNull(fixture.address)
            fixture.assertErased()
            fixture.assertPending()
        }
    }

    @Test fun liveUnlockPromotesTheExactAuthenticatedPendingRecord() {
        Fixture(persisted = true).use { fixture ->
            fixture.makePending()
            fixture.unlock()
            assertNull(fixture.failure)
            assertEquals(fixture.expected, fixture.address)
            fixture.assertErased()
            fixture.assertCommitted()
        }
    }

    private fun replacementDuringDecryption(pending: Boolean) {
        Fixture(persisted = true).use { fixture ->
            if (pending) fixture.makePending()
            val ciphertext = checkNotNull(fixture.prepared.record).ciphertext.copyOf()
            val entered = CountDownLatch(1)
            val release = CountDownLatch(1)
            fixture.calls.afterFinal = {
                entered.countDown()
                check(release.await(10, TimeUnit.SECONDS))
            }
            val replacement = try {
                fixture.startUnlock()
                assertTrue(entered.await(5, TimeUnit.SECONDS))
                assertArrayEquals(fixture.entropy, fixture.calls.output)
                fixture.replaceStoredRecord(pending)
            } finally { release.countDown() }
            fixture.awaitIdle()
            assertEquals(WalletProblem.OPERATION, fixture.failure)
            assertNull("Changed storage published an authenticated address", fixture.address)
            fixture.assertErased()
            fixture.assertErasedBeforeDispatch()
            assertArrayEquals(ciphertext, checkNotNull(fixture.prepared.record).ciphertext)
            fixture.assertReplacement(pending, replacement)
        }
    }

    @Test fun pendingRecordReplacementDuringDecryptionRefusesPublication() =
        replacementDuringDecryption(pending = true)

    @Test fun committedRecordReplacementDuringDecryptionRefusesPublication() =
        replacementDuringDecryption(pending = false)
}
