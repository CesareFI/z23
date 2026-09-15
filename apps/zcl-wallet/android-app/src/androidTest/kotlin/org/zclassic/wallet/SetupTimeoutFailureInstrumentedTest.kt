// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.os.Handler
import android.os.Looper
import android.os.Message
import android.view.View
import android.view.ViewGroup
import android.widget.LinearLayout
import android.widget.TextView
import androidx.test.core.app.ActivityScenario
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import java.io.File
import java.lang.reflect.InvocationTargetException
import java.nio.file.Files
import java.util.concurrent.CountDownLatch
import java.util.concurrent.Executor
import java.util.concurrent.ThreadPoolExecutor
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicLong
import java.util.concurrent.atomic.AtomicReference
import javax.crypto.Cipher
import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith
import org.zclassic.wallet.core.Network
import org.zclassic.wallet.core.WalletStorage

/** Unlaunched controller on the storage-free debug host. A real worker owns
 * only public marker entropy and an uninitialized cipher. No authentication,
 * key derivation, storage operation or real seed is used. */
@RunWith(AndroidJUnit4::class)
class SetupTimeoutFailureInstrumentedTest {
    private val instrumentation = InstrumentationRegistry.getInstrumentation()

    private fun field(name: String) = MainActivity::class.java.getDeclaredField(name).apply { isAccessible = true }

    private fun onMain(action: () -> Unit) {
        val failure = AtomicReference<Throwable?>()
        instrumentation.runOnMainSync {
            try { action() } catch (problem: Throwable) { failure.set(problem) }
        }
        failure.get()?.let { throw it }
    }

    private class PostingHandler(var enqueue: Boolean, var problem: Throwable?) : Handler(Looper.getMainLooper()) {
        var timeout: Runnable? = null
        override fun sendMessageAtTime(message: Message, uptimeMillis: Long): Boolean {
            timeout = message.callback
            val accepted = enqueue && super.sendMessageAtTime(message, uptimeMillis)
            check(!enqueue || accepted)
            problem?.let { throw it }
            return accepted
        }
    }

    private inner class Fixture(val host: WalletDisplayFixtureActivity) : AutoCloseable {
        private val parent = Files.createTempDirectory(host.cacheDir.toPath(), "public-setup-timeout-").toFile()
        private val directory = File(parent, "untouched")
        val controller = MainActivity()
        val screens = WalletScreens(host)
        val handler = PostingHandler(false, null)
        val clock = AtomicLong(100)
        val clockFailure = AtomicReference<Throwable?>()
        val window = SetupWindow { clockFailure.get()?.let { throw it }; clock.get() }
        val entropy = ByteArray(16) { 0x61 }
        val session = WalletPlatformSession(host, WalletStorage(directory.absolutePath),
            Executor { error("No wallet UI work is submitted") })
        val worker = WalletPlatformSession::class.java.getDeclaredField("work").apply { isAccessible = true }
            .get(session) as OwnedExecutor
        private val setup = WalletPlatformSession::class.java.getDeclaredField("setup").apply { isAccessible = true }
        private val workerFailure = AtomicReference<Throwable?>()
        private val release = CountDownLatch(1)
        private var backend: ThreadPoolExecutor? = null
        private val content = WalletScreens::class.java.getDeclaredField("content").apply { isAccessible = true }
            .get(screens) as LinearLayout

        init {
            field("screens").set(controller, screens)
            field("handler").set(controller, handler)
            field("session").set(controller, session)
            field("resumed").set(controller, true)
        }

        fun holdWorker() {
            val entered = CountDownLatch(1)
            assertTrue(worker.submit {
                try {
                    val type = WalletPlatformSession::class.java.declaredClasses.single { it.simpleName == "Setup" }
                    val constructor = type.declaredConstructors.single { it.parameterCount == 4 }.apply { isAccessible = true }
                    val prepared = PreparedWalletAction(WalletAction.CREATE,
                        Cipher.getInstance("AES/GCM/NoPadding"), Network.TESTNET)
                    setup.set(session, constructor.newInstance(prepared, window, entropy, ByteArray(80)))
                    entered.countDown()
                    check(release.await(10, TimeUnit.SECONDS)) { "Public worker gate expired" }
                } catch (problem: Throwable) { workerFailure.set(problem) }
                finally { entered.countDown() }
            })
            assertTrue(entered.await(5, TimeUnit.SECONDS))
            workerFailure.get()?.let { throw it }
            backend = OwnedExecutor::class.java.getDeclaredField("executor").apply { isAccessible = true }
                .get(worker) as ThreadPoolExecutor
        }

        fun start(): Boolean {
            try {
                return MainActivity::class.java.getDeclaredMethod("startSetupTimeout", SetupWindow::class.java)
                    .apply { isAccessible = true }.invoke(controller, window) as Boolean
            } catch (wrapped: InvocationTargetException) { throw checkNotNull(wrapped.cause) }
        }

        fun failRendering(problem: Throwable) {
            content.setOnHierarchyChangeListener(object : ViewGroup.OnHierarchyChangeListener {
                override fun onChildViewAdded(parent: View?, child: View?) { throw problem }
                override fun onChildViewRemoved(parent: View?, child: View?) = Unit
            })
        }

        fun assertRetired() {
            assertTrue("Failed scheduling left the secret-owning worker open", worker.isClosed)
            assertNull(field("session").get(controller))
            assertNull(field("setupWindow").get(controller))
            assertNull(field("setupTimeout").get(controller))
            handler.timeout?.let { assertFalse(handler.hasCallbacks(it)) }
            assertFalse(directory.exists())
        }

        fun assertMessage(identifier: Int) {
            assertEquals(host.getString(identifier), host.findViewById<TextView>(R.id.status_message).text.toString())
        }

        fun finishWorker() {
            release.countDown()
            assertTrue(checkNotNull(backend).awaitTermination(5, TimeUnit.SECONDS))
            workerFailure.get()?.let { throw it }
            assertNull(setup.get(session))
            assertTrue("Retired worker retained public marker entropy", entropy.all { it == 0.toByte() })
            assertFalse(directory.exists())
        }

        override fun close() {
            release.countDown()
            onMain {
                content.setOnHierarchyChangeListener(null)
                MainActivity::class.java.getDeclaredMethod("clearSetupTimeout").apply { isAccessible = true }.invoke(controller)
                session.close()
                handler.removeCallbacksAndMessages(null)
                screens.clearSecrets()
            }
            backend?.let { assertTrue(it.awaitTermination(5, TimeUnit.SECONDS)) }
            entropy.fill(0)
            assertTrue("Unexpected files in public fixture parent", parent.delete())
        }
    }

    private fun withFixture(action: (Fixture) -> Unit) {
        ActivityScenario.launch(WalletDisplayFixtureActivity::class.java).use { scenario ->
            val host = AtomicReference<WalletDisplayFixtureActivity>()
            scenario.onActivity { host.set(it) }
            val fixture = AtomicReference<Fixture>()
            onMain { fixture.set(Fixture(host.get())) }
            fixture.get().use { owned -> owned.holdWorker(); action(owned) }
        }
    }

    private fun failedSetup(enqueue: Boolean, fatal: Boolean, renderingFailure: Boolean = false,
                            clockFailure: Boolean = false) = withFixture { fixture ->
        val problem = if (fatal) OutOfMemoryError("Public setup scheduling failure")
            else IllegalStateException("Public setup scheduling refusal")
        onMain {
            fixture.handler.enqueue = enqueue
            if (clockFailure) fixture.clockFailure.set(problem) else fixture.handler.problem = problem
            if (renderingFailure) fixture.failRendering(OutOfMemoryError("Public secondary rendering failure"))
            if (fatal) assertSame(problem, assertThrows(Error::class.java) { fixture.start() })
            else assertFalse(fixture.start())
            fixture.assertRetired()
            if (!renderingFailure) fixture.assertMessage(R.string.operation_failed)
        }
        fixture.finishWorker()
    }

    @Test fun schedulingExceptionsRetireTheSecretOwnerBeforeAndAfterEnqueue() {
        failedSetup(enqueue = false, fatal = false)
        failedSetup(enqueue = true, fatal = false)
    }

    @Test fun schedulingErrorsRetireTheSecretOwnerBeforeAndAfterEnqueue() {
        failedSetup(enqueue = false, fatal = true)
        failedSetup(enqueue = true, fatal = true)
    }

    @Test fun secondaryRenderingFailurePreservesTheSetupError() =
        failedSetup(enqueue = true, fatal = true, renderingFailure = true)

    @Test fun clockFailureRetiresSetupBeforePosting() {
        failedSetup(enqueue = false, fatal = false, clockFailure = true)
        failedSetup(enqueue = false, fatal = true, clockFailure = true)
    }

    @Test fun falsePostStillClosesTheWorkerAndReportsRefusal() = withFixture { fixture ->
        onMain { assertFalse(fixture.start()); fixture.assertRetired(); fixture.assertMessage(R.string.operation_failed) }
        fixture.finishWorker()
    }

    @Test fun exactExpiryClosesTheWorkerWithoutPosting() = withFixture { fixture ->
        onMain {
            fixture.clock.set(600_100)
            assertFalse(fixture.start())
            fixture.assertRetired()
            assertNull(fixture.handler.timeout)
            fixture.assertMessage(R.string.setup_expired)
        }
        fixture.finishWorker()
    }

    @Test fun successfulSchedulingRetainsOnlyTheOriginalBoundedSetup() = withFixture { fixture ->
        onMain {
            fixture.handler.enqueue = true
            assertTrue(fixture.start())
            assertFalse(fixture.worker.isClosed)
            assertSame(fixture.session, field("session").get(fixture.controller))
            assertSame(fixture.window, field("setupWindow").get(fixture.controller))
            assertNotNull(fixture.handler.timeout)
            assertTrue(fixture.handler.hasCallbacks(fixture.handler.timeout!!))
            assertTrue(fixture.entropy.all { it == 0x61.toByte() })
        }
    }
}
