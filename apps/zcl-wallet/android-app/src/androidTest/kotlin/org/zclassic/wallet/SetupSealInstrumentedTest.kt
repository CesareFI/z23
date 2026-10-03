// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import java.io.File
import java.nio.file.Files
import java.util.concurrent.ConcurrentLinkedQueue
import java.util.concurrent.CountDownLatch
import java.util.concurrent.Executor
import java.util.concurrent.ThreadPoolExecutor
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicLong
import javax.crypto.Cipher
import javax.crypto.spec.GCMParameterSpec
import javax.crypto.spec.SecretKeySpec
import org.junit.Assert.assertEquals
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.zclassic.wallet.core.CoreStatus
import org.zclassic.wallet.core.Network
import org.zclassic.wallet.core.TransparentAddress
import org.zclassic.wallet.core.WalletKeys
import org.zclassic.wallet.core.WalletRecord
import org.zclassic.wallet.core.WalletStorage

/** Actual worker/confirmation/seal/storage routing with fixed public entropy,
 * key and IV. No entropy generation, Keystore alias or prompt is involved. */
@RunWith(AndroidJUnit4::class)
class SetupSealInstrumentedTest {
    @Test fun observerDelegatesPublicGcmParametersAndEncryption() {
        val calls = SetupObservedCipher()
        val cipher = calls.create()
        cipher.init(Cipher.ENCRYPT_MODE, SecretKeySpec(ByteArray(32), "AES"), GCMParameterSpec(128, ByteArray(12)))
        val parameters = checkNotNull(cipher.parameters).getParameterSpec(GCMParameterSpec::class.java)
        assertEquals(128, parameters.tLen)
        assertEquals(12, parameters.iv.size)
        cipher.updateAAD(ByteArray(80))
        assertEquals(32, cipher.doFinal(ByteArray(16) { 0x61 }).size)
        assertEquals(1, calls.aadCalls)
        assertEquals(1, calls.finalCalls)
    }

    private class Fixture(private val permitsStorage: Boolean = false,
                          private val action: WalletAction = WalletAction.CREATE) : AutoCloseable {
        private val context = ApplicationProvider.getApplicationContext<Context>()
        private val parent = Files.createTempDirectory(context.cacheDir.toPath(), "public-setup-seal-").toFile()
        private val directory = File(parent, "fixture")
        val storage = WalletStorage(directory.absolutePath)
        private val callbacks = ConcurrentLinkedQueue<Runnable>()
        private val erasedAtDispatch = ConcurrentLinkedQueue<Boolean>()
        val calls = SetupObservedCipher()
        val session = WalletPlatformSession(context, storage, Executor {
            calls.input?.let { input -> erasedAtDispatch.add(input.all { byte -> byte == 0.toByte() }) }
            callbacks.add(it)
        })
        val entropy = ByteArray(16) { 0x61 } // Nonzero public marker makes erasure observable.
        val words = WalletKeys.recoveryPhrase(entropy)
        val clock = AtomicLong(100)
        var beforeClockRead: (() -> Unit)? = null
        var failure: WalletProblem? = null
        var address: TransparentAddress? = null
        private val setupField = WalletPlatformSession::class.java.getDeclaredField("setup").apply { isAccessible = true }
        private val worker = WalletPlatformSession::class.java.getDeclaredField("work").apply { isAccessible = true }
            .get(session) as OwnedExecutor

        init {
            val cipher = calls.create()
            cipher.init(Cipher.ENCRYPT_MODE, SecretKeySpec(ByteArray(32), "AES"), GCMParameterSpec(128, ByteArray(12)))
            val prepared = PreparedWalletAction(action, cipher, Network.TESTNET)
            val type = WalletPlatformSession::class.java.declaredClasses.single { it.simpleName == "Setup" }
            val constructor = type.declaredConstructors.single { it.parameterCount == 4 }.apply { isAccessible = true }
            setupField.set(session, constructor.newInstance(prepared, SetupWindow {
                beforeClockRead?.invoke()
                clock.get()
            },
                if (action == WalletAction.CREATE) entropy else null,
                WalletRecord.createHeader(entropy, Network.TESTNET)))
        }

        fun start() {
            if (action == WalletAction.CREATE) {
                session.confirmCreation(words, { error("Canonical public confirmation was refused") },
                    { address = it }, { failure = it })
            } else {
                session.restore(words, { error("Canonical public restoration was refused") },
                    { address = it }, { failure = it })
            }
        }

        fun submit(deliver: Boolean = true) {
            start()
            val idle = CountDownLatch(1)
            assertTrue(worker.submit { idle.countDown() })
            assertTrue(idle.await(10, TimeUnit.SECONDS))
            if (deliver) deliverCallbacks()
        }

        fun deliverCallbacks() { while (true) (callbacks.poll() ?: break).run() }

        fun assertStored(record: ByteArray) {
            val stored = storage.read()
            assertEquals(CoreStatus.OK, stored.status)
            assertFalse(stored.pending)
            assertArrayEquals(record, stored.record)
            val change = File(directory, ".change.index")
            if (action == WalletAction.CREATE) assertEquals(80L, change.length())
            else assertFalse(change.exists()) // Restore cannot reset historical indexes.
        }

        fun awaitRetired() {
            val backend = OwnedExecutor::class.java.getDeclaredField("executor").apply { isAccessible = true }
                .get(worker) as ThreadPoolExecutor
            assertTrue(backend.awaitTermination(5, TimeUnit.SECONDS))
            while (true) (callbacks.poll() ?: break).run()
        }

        fun assertCancelled() {
            assertNull(failure)
            assertNull(address)
            assertFalse("Closed setup started persistence after encryption", directory.exists())
            assertCleared()
        }

        fun assertCleared() {
            assertNull(setupField.get(session))
            if (action == WalletAction.CREATE) assertTrue(entropy.all { it == 0.toByte() })
            else {
                assertTrue(entropy.all { it == 0x61.toByte() }) // Original public marker was borrowed by the fixture.
                assertTrue(checkNotNull(calls.input).all { it == 0.toByte() })
            }
            assertTrue(words.all { it == '\u0000' })
        }

        fun assertErasedBeforeDispatch() {
            assertEquals("Sealed entropy survived into the UI handoff", listOf(true), erasedAtDispatch.toList())
        }

        fun assertRefused() {
            assertEquals(WalletProblem.OPERATION, failure)
            assertNull(address)
            assertFalse("Refused sealing touched storage", directory.exists())
            assertCleared()
        }

        override fun close() {
            session.close()
            val backend = OwnedExecutor::class.java.getDeclaredField("executor").apply { isAccessible = true }
                .get(worker) as ThreadPoolExecutor?
            if (backend != null) assertTrue(backend.awaitTermination(5, TimeUnit.SECONDS))
            entropy.fill(0)
            calls.input?.fill(0)
            words.fill('\u0000')
            if (permitsStorage) {
                // Only the success fixture's known files. Unexpected data is
                // retained as evidence; never recursively delete wallet paths.
                for (name in listOf("wallet.zcl", ".wallet.pending", ".change.index", ".lock")) {
                    val file = File(directory, name)
                    if (file.exists()) assertTrue(file.delete())
                }
                if (directory.exists()) assertTrue(directory.delete())
            }
            assertTrue("Public sealing fixture unexpectedly retained files", parent.delete())
        }
    }

    @Test fun expiredCreationDeliveryPreservesStorageButRefusesReceive() =
        delayedResult(WalletAction.CREATE)

    @Test fun expiredRestoreDeliveryPreservesStorageButRefusesReceive() =
        delayedResult(WalletAction.RESTORE)

    private fun delayedResult(action: WalletAction) {
        for (now in listOf(99L, 600_100L, Long.MAX_VALUE))
            Fixture(permitsStorage = true, action = action).use { fixture ->
                fixture.submit(deliver = false)
                val record = checkNotNull(fixture.storage.read().record)
                fixture.assertStored(record)
                fixture.assertCleared()
                fixture.assertErasedBeforeDispatch()
                assertNull(fixture.address)
                fixture.clock.set(now)
                fixture.deliverCallbacks()
                assertEquals(WalletProblem.OPERATION, fixture.failure)
                assertNull(fixture.address)
                fixture.assertStored(record)
                fixture.assertCleared()
            }
    }

    @Test fun lastLiveSetupMillisecondStillDeliversCreationAndRestore() {
        for (action in listOf(WalletAction.CREATE, WalletAction.RESTORE))
            Fixture(permitsStorage = true, action = action).use { fixture ->
                val expected = WalletKeys.receivingAddress(fixture.entropy, Network.TESTNET)
                fixture.submit(deliver = false)
                val record = checkNotNull(fixture.storage.read().record)
                fixture.clock.set(600_099)
                fixture.deliverCallbacks()
                assertNull(fixture.failure)
                assertEquals(expected, fixture.address)
                fixture.assertStored(record)
                fixture.assertCleared()
            }
    }

    @Test fun expiryAfterGcmCompletionRefusesPersistenceAndClearsOwners() {
        Fixture().use { fixture ->
            fixture.calls.afterFinal = { fixture.clock.set(600_100) }
            fixture.submit()
            assertEquals(1, fixture.calls.aadCalls)
            assertEquals(1, fixture.calls.finalCalls)
            assertSame(fixture.entropy, fixture.calls.input)
            fixture.assertRefused()
        }
    }

    @Test fun aadFailureClearsSetupAndSubmittedWordsWithoutFinalizing() {
        Fixture().use { fixture ->
            fixture.calls.afterAad = { throw IllegalStateException("Injected public AAD failure") }
            fixture.submit()
            assertEquals(1, fixture.calls.aadCalls)
            assertEquals(0, fixture.calls.finalCalls)
            fixture.assertRefused()
        }
    }

    @Test fun finalizationFailureClearsBorrowedEntropyAndSubmittedWords() {
        Fixture().use { fixture ->
            fixture.calls.beforeFinal = { throw IllegalStateException("Injected public GCM failure") }
            fixture.submit()
            assertEquals(1, fixture.calls.finalCalls)
            assertSame(fixture.entropy, fixture.calls.input)
            fixture.assertRefused()
        }
    }

    @Test fun liveSealCommitsThePublicFixtureAndClearsOwners() {
        Fixture(permitsStorage = true).use { fixture ->
            val expected = WalletKeys.receivingAddress(fixture.entropy, Network.TESTNET)
            fixture.submit()
            assertNull(fixture.failure)
            assertEquals(expected, fixture.address)
            assertEquals(CoreStatus.OK, fixture.storage.read().status)
            assertSame(fixture.entropy, fixture.calls.input)
            fixture.assertCleared()
            fixture.assertErasedBeforeDispatch()
        }
    }

    @Test fun confirmedWordsAreErasedBeforeEncryptionBegins() = checkPhraseRetirement(WalletAction.CREATE)

    @Test fun restoredWordsAreErasedBeforeEncryptionBegins() = checkPhraseRetirement(WalletAction.RESTORE)

    @Test fun closingDuringCreationEncryptionRefusesNewPersistence() = checkClosedSeal(WalletAction.CREATE)

    @Test fun closingDuringRestorationEncryptionRefusesNewPersistence() = checkClosedSeal(WalletAction.RESTORE)

    private fun checkClosedSeal(action: WalletAction) {
        Fixture(permitsStorage = true, action = action).use { fixture ->
            val entered = CountDownLatch(1)
            val release = CountDownLatch(1)
            fixture.calls.afterFinal = {
                entered.countDown()
                check(release.await(10, TimeUnit.SECONDS))
            }
            try {
                fixture.start()
                assertTrue(entered.await(5, TimeUnit.SECONDS))
                InstrumentationRegistry.getInstrumentation().runOnMainSync { fixture.session.close() }
                // The provider still owns its input. The worker must clear it
                // after returning, without beginning a new storage operation.
                assertTrue(checkNotNull(fixture.calls.input).all { it == 0x61.toByte() })
            } finally { release.countDown() }
            fixture.awaitRetired()
            assertEquals(1, fixture.calls.finalCalls)
            fixture.assertCancelled()
        }
    }

    @Test fun liveRestoreCommitsTheSamePublicAddressAndClearsDecodedEntropy() {
        Fixture(permitsStorage = true, action = WalletAction.RESTORE).use { fixture ->
            val expected = WalletKeys.receivingAddress(fixture.entropy, Network.TESTNET)
            fixture.submit()
            assertNull(fixture.failure)
            assertEquals(expected, fixture.address)
            assertEquals(CoreStatus.OK, fixture.storage.read().status)
            fixture.assertCleared()
            fixture.assertErasedBeforeDispatch()
        }
    }

    @Test fun restoredEntropyRetiresBeforeStorageAdmission() {
        Fixture(permitsStorage = true, action = WalletAction.RESTORE).use { fixture ->
            val expected = WalletKeys.receivingAddress(fixture.entropy, Network.TESTNET)
            var observed = false
            var erased = false
            fixture.calls.afterFinal = {
                fixture.beforeClockRead = {
                    fixture.beforeClockRead = null
                    observed = true
                    erased = checkNotNull(fixture.calls.input).all { it == 0.toByte() }
                }
            }
            fixture.submit()
            assertTrue("Storage admission was not observed", observed)
            assertTrue("Restored entropy survived its last cryptographic use", erased)
            assertNull(fixture.failure)
            assertEquals(expected, fixture.address)
            fixture.assertStored(checkNotNull(fixture.storage.read().record))
            fixture.assertCleared()
        }
    }

    private fun checkPhraseRetirement(action: WalletAction) {
        Fixture(action = action).use { fixture ->
            var wordsClearedAtCipher = false
            var entropyPresentAtCipher = false
            fixture.calls.beforeFinal = {
                wordsClearedAtCipher = fixture.words.all { it == '\u0000' }
                entropyPresentAtCipher = checkNotNull(fixture.calls.input).all { it == 0x61.toByte() }
                throw IllegalStateException("Public injected failure after observing input ownership")
            }
            fixture.submit()
            assertEquals(1, fixture.calls.finalCalls)
            fixture.assertRefused()
            assertTrue("Consumed recovery words remained live during encryption", wordsClearedAtCipher)
            assertTrue("Encryption lost its active entropy", entropyPresentAtCipher)
        }
    }
}
