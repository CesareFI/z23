// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import androidx.test.ext.junit.runners.AndroidJUnit4
import java.io.File
import java.nio.file.Files
import java.util.concurrent.ConcurrentLinkedQueue
import java.util.concurrent.CountDownLatch
import java.util.concurrent.Executor
import java.util.concurrent.ThreadPoolExecutor
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicLong
import javax.crypto.Cipher
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.zclassic.wallet.core.Network
import org.zclassic.wallet.core.WalletStorage

/** Public marker arrays, an uninitialized cipher and a unique empty cache
 * parent. Expiry must refuse before reading storage, deriving or encrypting.
 * No prompt, key generation, real seed or hardware acceptance is involved. */
@RunWith(AndroidJUnit4::class)
class SetupWorkerExpiryInstrumentedTest {
    private class Fixture : AutoCloseable {
        private val context = ApplicationProvider.getApplicationContext<Context>()
        private val parent = Files.createTempDirectory(context.cacheDir.toPath(), "public-setup-expiry-").toFile()
        private val directory = File(parent, "untouched")
        val callbacks = ConcurrentLinkedQueue<Runnable>()
        val session = WalletPlatformSession(context, WalletStorage(directory.absolutePath), Executor { callbacks.add(it) })
        val clock = AtomicLong(100)
        val window = SetupWindow(clock::get)
        val entropy = ByteArray(16) { 0x61 }
        val words = charArrayOf('a', 'b', 'c') // Invalid public phrase.
        private val setupField = WalletPlatformSession::class.java.getDeclaredField("setup").apply { isAccessible = true }
        val worker = WalletPlatformSession::class.java.getDeclaredField("work").apply { isAccessible = true }
            .get(session) as OwnedExecutor
        private val release = CountDownLatch(1)

        fun prepared(action: WalletAction) = PreparedWalletAction(action,
            Cipher.getInstance("AES/GCM/NoPadding"), Network.TESTNET)

        fun existingSetup(action: WalletAction) {
            val type = WalletPlatformSession::class.java.declaredClasses.single { it.simpleName == "Setup" }
            val constructor = type.declaredConstructors.single { it.parameterCount == 4 }.apply { isAccessible = true }
            setupField.set(session, constructor.newInstance(prepared(action), window, entropy, ByteArray(80)))
        }

        fun holdWorker() {
            val entered = CountDownLatch(1)
            assertTrue(worker.submit {
                entered.countDown()
                check(release.await(10, TimeUnit.SECONDS))
            })
            assertTrue(entered.await(5, TimeUnit.SECONDS))
        }

        fun expireAndDrain(at: Long = 600_100) {
            clock.set(at)
            release.countDown()
            val idle = CountDownLatch(1)
            assertTrue(worker.submit { idle.countDown() })
            assertTrue(idle.await(5, TimeUnit.SECONDS))
            while (true) (callbacks.poll() ?: break).run()
        }

        fun assertCleared() {
            assertNull(setupField.get(session))
            assertTrue(entropy.all { it == 0.toByte() })
            assertTrue(words.all { it == '\u0000' })
            assertFalse("Expired work touched wallet storage", directory.exists())
        }

        fun assertRetained() {
            assertTrue(setupField.get(session) != null)
            assertTrue(entropy.all { it == 0x61.toByte() })
            assertTrue(words.all { it == '\u0000' })
            assertFalse(directory.exists())
        }

        override fun close() {
            release.countDown()
            session.close()
            val backend = OwnedExecutor::class.java.getDeclaredField("executor").apply { isAccessible = true }
                .get(worker) as ThreadPoolExecutor?
            if (backend != null) assertTrue(backend.awaitTermination(5, TimeUnit.SECONDS))
            entropy.fill(0)
            words.fill('\u0000')
            // Delete only the empty parent we created. Unexpected files stay as
            // failure evidence; never recursively delete a wallet directory.
            assertTrue("Public expiry fixture unexpectedly created files", parent.delete())
        }
    }

    @Test fun queuedConfirmationExpiresBeforePhraseComparisonAndClearsItsOwners() {
        Fixture().use { fixture ->
            fixture.existingSetup(WalletAction.CREATE)
            fixture.holdWorker()
            var failure: WalletProblem? = null
            fixture.session.confirmCreation(fixture.words,
                { error("Expired confirmation reached phrase comparison") },
                { error("Expired confirmation completed") }, { failure = it })
            fixture.expireAndDrain()
            assertEquals(WalletProblem.OPERATION, failure)
            fixture.assertCleared()
        }
    }

    @Test fun queuedRestoreExpiresBeforeDecodingAndClearsItsOwners() {
        Fixture().use { fixture ->
            fixture.existingSetup(WalletAction.RESTORE)
            fixture.holdWorker()
            var failure: WalletProblem? = null
            fixture.session.restore(fixture.words, { error("Expired restoration reached decoding") },
                { error("Expired restoration completed") }, { failure = it })
            fixture.expireAndDrain()
            assertEquals(WalletProblem.OPERATION, failure)
            fixture.assertCleared()
        }
    }

    @Test fun queuedRestoreAdmissionExpiresBeforeStorageRead() {
        Fixture().use { fixture ->
            fixture.holdWorker()
            var failure: WalletProblem? = null
            fixture.session.restoreAfterAuthentication(fixture.prepared(WalletAction.RESTORE), fixture.window,
                { error("Expired restoration opened setup") }, { failure = it })
            fixture.expireAndDrain()
            assertEquals(WalletProblem.OPERATION, failure)
            assertTrue(fixture.entropy.all { it == 0x61.toByte() }) // Never transferred to the session.
            fixture.entropy.fill(0)
            fixture.words.fill('\u0000')
            fixture.assertCleared()
        }
    }

    @Test fun liveMalformedConfirmationRetainsSetupAndItsOriginalDeadline() {
        Fixture().use { fixture ->
            fixture.existingSetup(WalletAction.CREATE)
            fixture.holdWorker()
            var mismatch = false
            fixture.session.confirmCreation(fixture.words, { mismatch = true },
                { error("Malformed public confirmation completed") }, { error("Live setup was refused") })
            fixture.expireAndDrain(at = 600_099)
            assertTrue(mismatch)
            fixture.assertRetained()
            fixture.clock.set(600_100)
            assertEquals(0L, fixture.window.remainingMillis)
        }
    }

    @Test fun liveMalformedRestoreRetainsSetupAndItsOriginalDeadline() {
        Fixture().use { fixture ->
            fixture.existingSetup(WalletAction.RESTORE)
            fixture.holdWorker()
            var invalid = false
            fixture.session.restore(fixture.words, { invalid = true },
                { error("Malformed public restoration completed") }, { error("Live setup was refused") })
            fixture.expireAndDrain(at = 600_099)
            assertTrue(invalid)
            fixture.assertRetained()
            fixture.clock.set(600_100)
            assertEquals(0L, fixture.window.remainingMillis)
        }
    }
}
