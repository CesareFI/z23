// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.system.Os
import android.view.View
import android.widget.TextView
import androidx.test.core.app.ActivityScenario
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import java.io.File
import java.lang.reflect.InvocationTargetException
import java.nio.file.Files
import java.util.concurrent.Executor
import java.util.concurrent.LinkedBlockingQueue
import java.util.concurrent.ThreadPoolExecutor
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicReference
import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith
import org.zclassic.wallet.core.Network
import org.zclassic.wallet.core.WalletRecord
import org.zclassic.wallet.core.WalletStorage

/** Real native read, session, controller and views, with a bounded intercepted
 * UI handoff. The unattached controller uses only an invocation-owned cache
 * directory and the existing debug view host. No prompt, Keystore, wallet-v1,
 * production seed or authentication claim; record ciphertext is inert. */
@RunWith(AndroidJUnit4::class)
class StorageAdmissionInstrumentedTest {
    private val instrumentation = InstrumentationRegistry.getInstrumentation()
    private fun field(name: String) = MainActivity::class.java.getDeclaredField(name).apply { isAccessible = true }

    private fun onMain(action: () -> Unit) {
        val failure = AtomicReference<Throwable?>()
        instrumentation.runOnMainSync {
            try { action() } catch (problem: Throwable) { failure.set(problem) }
        }
        failure.get()?.let { throw it }
    }

    private fun publicRecord(): ByteArray {
        val entropy = ByteArray(16)
        try {
            val header = WalletRecord.createHeader(entropy, Network.TESTNET)
            return WalletRecord.pack(header, ByteArray(12), ByteArray(32))
        } finally { entropy.fill(0) }
    }

    private inner class Fixture(val host: WalletDisplayFixtureActivity) : AutoCloseable {
        private val parent = Files.createTempDirectory(host.cacheDir.toPath(), "public-storage-admission-").toFile()
        private val directory = File(parent, "store")
        private val names = listOf("wallet.zcl", ".wallet.pending", ".change.index", ".lock")
        private val callbacks = LinkedBlockingQueue<Runnable>(1)
        private val controller = MainActivity()
        private val screens = WalletScreens(host)
        private val session = WalletPlatformSession(host, WalletStorage(directory.absolutePath),
            Executor { check(callbacks.offer(it)) { "Unexpected second inspection callback" } })
        private val worker = WalletPlatformSession::class.java.getDeclaredField("work").apply { isAccessible = true }
            .get(session) as OwnedExecutor

        init {
            assertTrue(directory.mkdir())
            Os.chmod(directory.absolutePath, 0x1c0) // 0700
            field("screens").set(controller, screens)
            field("session").set(controller, session)
            field("resumed").set(controller, true)
        }

        fun write(name: String, bytes: ByteArray) {
            check(name in names && name != ".lock")
            File(directory, name).apply { writeBytes(bytes); Os.chmod(absolutePath, 0x180) }
        }

        fun bytes(name: String): ByteArray = File(directory, name).readBytes()
        fun exists(name: String): Boolean = File(directory, name).exists()
        fun makeRecordPublic() { Os.chmod(File(directory, "wallet.zcl").absolutePath, 0x1a4) } // 0644

        fun start(): Runnable {
            onMain {
                try {
                    MainActivity::class.java.getDeclaredMethod("inspect").apply { isAccessible = true }.invoke(controller)
                } catch (wrapped: InvocationTargetException) { throw checkNotNull(wrapped.cause) }
                assertMessage(R.string.wallet_working)
                assertNull(host.findViewById<View>(R.id.create_wallet))
            }
            return checkNotNull(callbacks.poll(5, TimeUnit.SECONDS)) { "Inspection callback did not arrive" }
        }

        fun finish(callback: Runnable) = onMain {
            callback.run()
            assertTrue(callbacks.isEmpty())
        }

        fun assertMessage(identifier: Int) {
            assertEquals(host.getString(identifier), host.findViewById<TextView>(R.id.status_message).text.toString())
        }

        fun assertNoSetup() {
            assertNull(host.findViewById<View>(R.id.create_wallet))
            assertNull(host.findViewById<View>(R.id.restore_wallet))
            assertNull(host.findViewById<View>(R.id.recovery_words))
            assertNull(host.findViewById<View>(R.id.recovery_input))
        }

        fun assertFailure() = onMain {
            assertMessage(R.string.storage_failed)
            assertNoSetup()
            assertNull(host.findViewById<View>(R.id.unlock_wallet))
            assertNull(field("session").get(controller))
            assertTrue(worker.isClosed)
            assertFalse(field("busy").getBoolean(controller))
        }

        fun closeSession() = onMain { session.close() }

        override fun close() {
            onMain { session.close(); screens.clearSecrets() }
            val backend = OwnedExecutor::class.java.getDeclaredField("executor").apply { isAccessible = true }
                .get(worker) as ThreadPoolExecutor?
            backend?.let { assertTrue("Inspection worker did not retire", it.awaitTermination(5, TimeUnit.SECONDS)) }
            assertTrue(checkNotNull(directory.list()).all { it in names })
            names.forEach { Files.deleteIfExists(File(directory, it).toPath()) }
            assertTrue(directory.delete())
            assertTrue(parent.delete())
        }
    }

    private fun withFixture(action: (Fixture) -> Unit) {
        ActivityScenario.launch(WalletDisplayFixtureActivity::class.java).use { scenario ->
            val host = AtomicReference<WalletDisplayFixtureActivity>()
            scenario.onActivity { host.set(it) }
            val fixture = AtomicReference<Fixture>()
            onMain { fixture.set(Fixture(host.get())) }
            fixture.get().use(action)
        }
    }

    @Test fun emptyStorageOffersCreateAndRestoreAfterInspection() = withFixture { fixture ->
        fixture.finish(fixture.start())
        onMain {
            fixture.assertMessage(R.string.wallet_setup_description)
            assertNotNull(fixture.host.findViewById<View>(R.id.create_wallet))
            assertNotNull(fixture.host.findViewById<View>(R.id.restore_wallet))
            assertNull(fixture.host.findViewById<View>(R.id.unlock_wallet))
        }
        assertFalse(fixture.exists("wallet.zcl"))
        assertFalse(fixture.exists(".wallet.pending"))
        assertFalse(fixture.exists(".change.index"))
    }

    @Test fun orphanChangeStateShowsStorageFailureWithoutSetupOrFileMutation() {
        for (length in listOf(0, 40, 80)) withFixture { fixture ->
            val bytes = ByteArray(length) { 0x5a }
            fixture.write(".change.index", bytes)
            fixture.finish(fixture.start())
            fixture.assertFailure()
            assertArrayEquals(bytes, fixture.bytes(".change.index"))
            assertFalse(fixture.exists("wallet.zcl"))
            assertFalse(fixture.exists(".wallet.pending"))
        }
    }

    @Test fun corruptCommittedRecordCannotOfferPendingRecoveryOrSetup() = withFixture { fixture ->
        val pending = publicRecord()
        val damaged = byteArrayOf(1)
        fixture.write(".wallet.pending", pending)
        fixture.write("wallet.zcl", damaged)
        fixture.finish(fixture.start())
        fixture.assertFailure()
        assertArrayEquals(pending, fixture.bytes(".wallet.pending"))
        assertArrayEquals(damaged, fixture.bytes("wallet.zcl"))
    }

    @Test fun completePendingRecordOffersUnlockWithoutPromotingOrOfferingSetup() = withFixture { fixture ->
        val pending = publicRecord()
        fixture.write(".wallet.pending", pending)
        fixture.finish(fixture.start())
        onMain {
            fixture.assertMessage(R.string.wallet_pending)
            fixture.assertNoSetup()
            assertNotNull(fixture.host.findViewById<View>(R.id.unlock_wallet))
        }
        assertArrayEquals(pending, fixture.bytes(".wallet.pending"))
        assertFalse(fixture.exists("wallet.zcl"))
    }

    @Test fun publicRecordPermissionsShowFailureWithoutUnlockOrSetup() = withFixture { fixture ->
        val record = publicRecord()
        fixture.write("wallet.zcl", record)
        fixture.makeRecordPublic()
        fixture.finish(fixture.start())
        fixture.assertFailure()
        assertArrayEquals(record, fixture.bytes("wallet.zcl"))
        assertFalse(fixture.exists(".wallet.pending"))
    }

    @Test fun closedSessionCannotDeliverAnAlreadyQueuedSetupScreen() = withFixture { fixture ->
        val callback = fixture.start()
        fixture.closeSession()
        fixture.finish(callback)
        onMain {
            fixture.assertMessage(R.string.wallet_working)
            fixture.assertNoSetup()
            assertNull(fixture.host.findViewById<View>(R.id.unlock_wallet))
        }
    }
}
