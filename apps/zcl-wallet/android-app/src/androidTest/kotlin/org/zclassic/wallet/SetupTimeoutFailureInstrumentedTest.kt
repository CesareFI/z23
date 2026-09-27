// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.app.Activity
import android.app.Application
import android.content.ContextWrapper
import android.os.Bundle
import android.os.CancellationSignal
import android.os.Handler
import android.os.Looper
import android.os.Message
import android.text.Editable
import android.text.TextWatcher
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
 * only public marker entropy and an uninitialized cipher. The successful
 * restart reads an absent disposable wallet path, creating only its empty
 * lock file. No authentication, key derivation, wallet record or real seed is used. */
@RunWith(AndroidJUnit4::class)
class SetupTimeoutFailureInstrumentedTest {
    private val instrumentation = InstrumentationRegistry.getInstrumentation()

    private fun field(name: String) = MainActivity::class.java.getDeclaredField(name).apply { isAccessible = true }

    private class LifecycleApplication : Application(), Application.ActivityLifecycleCallbacks {
        var destructions = 0
        init { registerActivityLifecycleCallbacks(this) }
        override fun onActivityDestroyed(activity: Activity) { destructions++ }
        override fun onActivityCreated(activity: Activity, state: Bundle?) = Unit
        override fun onActivityStarted(activity: Activity) = Unit
        override fun onActivityResumed(activity: Activity) = Unit
        override fun onActivityPaused(activity: Activity) = Unit
        override fun onActivityStopped(activity: Activity) = Unit
        override fun onActivitySaveInstanceState(activity: Activity, state: Bundle) = Unit
    }

    private fun controller(): MainActivity = MainActivity().also {
        // No onCreate/onResume or storage routing. Supply framework lifecycle
        // dependencies only: an inert application and the test context.
        Activity::class.java.getDeclaredField("mApplication").apply { isAccessible = true }
            .set(it, LifecycleApplication())
        ContextWrapper::class.java.getDeclaredField("mBase").apply { isAccessible = true }
            .set(it, instrumentation.targetContext)
    }

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
        private val restartDirectory = File(parent, "restart-empty")
        val controller = controller()
        val screens = WalletScreens(host)
        val authentication = WalletAuthentication(host,
            { error("No authentication is performed") }, { error("No prompt is active") })
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
            field("authentication").set(controller, authentication)
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

        fun destroy() {
            try {
                MainActivity::class.java.getDeclaredMethod("onDestroy").apply { isAccessible = true }
                    .invoke(controller)
            } catch (wrapped: InvocationTargetException) { throw checkNotNull(wrapped.cause) }
        }

        fun fail() {
            try {
                MainActivity::class.java.getDeclaredMethod("showFailure", Int::class.javaPrimitiveType)
                    .apply { isAccessible = true }.invoke(controller, R.string.operation_failed)
            } catch (wrapped: InvocationTargetException) { throw checkNotNull(wrapped.cause) }
        }

        fun restart() {
            try {
                MainActivity::class.java.getDeclaredMethod("restart").apply { isAccessible = true }
                    .invoke(controller)
            } catch (wrapped: InvocationTargetException) { throw checkNotNull(wrapped.cause) }
        }

        fun failSessionClose(problem: Throwable) {
            assertTrue(worker.submit({ throw problem }) { error("Cancelled public cleanup fixture ran") })
        }

        fun failCancellation(problem: Throwable): CancellationSignal {
            val type = WalletAuthentication::class.java.declaredClasses.single { it.simpleName == "Pending" }
            val prepared = PreparedWalletAction(WalletAction.CREATE,
                Cipher.getInstance("AES/GCM/NoPadding"), Network.TESTNET)
            val pending = type.getDeclaredConstructor(PreparedWalletAction::class.java)
                .apply { isAccessible = true }.newInstance(prepared)
            WalletAuthentication::class.java.getDeclaredField("pending").apply { isAccessible = true }
                .set(authentication, pending)
            return (type.getDeclaredField("signal").apply { isAccessible = true }.get(pending)
                as CancellationSignal).also { signal -> signal.setOnCancelListener { throw problem } }
        }

        fun failRendering(problem: Throwable) {
            content.setOnHierarchyChangeListener(object : ViewGroup.OnHierarchyChangeListener {
                override fun onChildViewAdded(parent: View?, child: View?) { throw problem }
                override fun onChildViewRemoved(parent: View?, child: View?) = Unit
            })
        }

        fun observeEmptyRestart(welcome: CountDownLatch) {
            field("storage").set(controller, WalletStorage(restartDirectory.absolutePath))
            content.setOnHierarchyChangeListener(object : ViewGroup.OnHierarchyChangeListener {
                override fun onChildViewAdded(parent: View?, child: View?) {
                    if (child?.id == R.id.create_wallet) welcome.countDown()
                }
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
            if (restartDirectory.exists()) {
                val entries = checkNotNull(restartDirectory.listFiles())
                assertEquals(listOf(".lock"), entries.map { it.name })
                assertTrue(entries.single().isFile)
                assertEquals(0L, entries.single().length())
                assertTrue(entries.single().delete())
                assertTrue(restartDirectory.delete())
            }
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

    private fun shownSecret(fixture: Fixture, input: Boolean): Pair<CharArray, View> {
        if (!input) {
            val words = charArrayOf('a', 'b', 'c')
            fixture.screens.backup(words, {}, {})
            return words to fixture.host.findViewById<RecoveryWordsView>(R.id.recovery_words)
        }
        fixture.screens.enterRecovery(false, false, {}, {})
        val view = fixture.host.findViewById<RecoveryInputView>(R.id.recovery_input)
        for (character in "abc") view.append(character)
        val buffer = RecoveryInputView::class.java.getDeclaredField("characters")
            .apply { isAccessible = true }.get(view) as CharArray
        val preview = RecoveryInputView::class.java.getDeclaredField("preview")
            .apply { isAccessible = true }.get(view) as TextView
        return buffer to preview
    }

    private fun failedSessionClose(input: Boolean, rendering: Boolean) {
        for (fatal in listOf(false, true)) withFixture { fixture ->
            val problem = if (fatal) OutOfMemoryError("Public session cleanup failure")
                else IllegalStateException("Public session cleanup failure")
            onMain {
                fixture.handler.enqueue = true
                assertTrue(fixture.start())
                val (words, view) = shownSecret(fixture, input)
                try {
                    assertEquals(View.VISIBLE, view.visibility)
                    assertEquals("abc", (view as TextView).text.toString())
                    fixture.failSessionClose(problem)
                    if (rendering) fixture.failRendering(OutOfMemoryError("Public secondary failure UI error"))
                    assertSame(problem, assertThrows(Throwable::class.java) { fixture.fail() })
                    assertTrue("Failure left the displayed phrase owned by the view", words.all { it == '\u0000' })
                    assertEquals(View.INVISIBLE, view.visibility)
                    fixture.assertRetired()
                    assertFalse(field("busy").getBoolean(fixture.controller))
                    if (!rendering) fixture.assertMessage(R.string.operation_failed)
                } finally { words.fill('\u0000') }
            }
            fixture.finishWorker()
        }
    }

    @Test fun sessionCleanupFailureStillRetiresTheScreenAndOwner() {
        failedSessionClose(input = false, rendering = false)
        failedSessionClose(input = false, rendering = true)
    }

    @Test fun sessionCleanupFailureStillClearsRecoveryInput() {
        failedSessionClose(input = true, rendering = false)
        failedSessionClose(input = true, rendering = true)
    }

    private fun failedRestart(input: Boolean, cancellation: Boolean = false, clearing: Boolean = false) {
        for (fatal in listOf(false, true)) withFixture { fixture ->
            val problem = if (fatal) OutOfMemoryError("Public restart cleanup failure")
                else IllegalStateException("Public restart cleanup failure")
            onMain {
                fixture.handler.enqueue = true
                assertTrue(fixture.start())
                val (words, view) = shownSecret(fixture, input)
                val listener = object : TextWatcher {
                    override fun beforeTextChanged(text: CharSequence?, start: Int, count: Int, after: Int) {
                        if (after == 0) throw OutOfMemoryError("Public secondary restart view failure")
                    }
                    override fun onTextChanged(text: CharSequence?, start: Int, before: Int, count: Int) = Unit
                    override fun afterTextChanged(text: Editable?) = Unit
                }
                try {
                    assertEquals(View.VISIBLE, view.visibility)
                    assertEquals("abc", (view as TextView).text.toString())
                    val signal = if (cancellation) fixture.failCancellation(problem) else null
                    fixture.failSessionClose(if (cancellation)
                        IllegalStateException("Public secondary restart cleanup failure") else problem)
                    if (clearing) view.addTextChangedListener(listener)
                    assertSame(problem, assertThrows(Throwable::class.java) { fixture.restart() })
                    assertTrue("Restart left the displayed phrase owned by the view", words.all { it == '\u0000' })
                    assertEquals(View.INVISIBLE, view.visibility)
                    fixture.assertRetired()
                    assertFalse(fixture.authentication.hasPending)
                    signal?.let { assertTrue(it.isCanceled) }
                    assertTrue(field("busy").getBoolean(fixture.controller))
                    assertTrue(fixture.entropy.all { it == 0x61.toByte() })
                } finally { (view as TextView).removeTextChangedListener(listener); words.fill('\u0000') }
            }
            fixture.finishWorker()
        }
    }

    @Test fun restartCleanupFailureStillRetiresTheScreenAndOwner() = failedRestart(input = false)

    @Test fun restartCleanupFailureStillClearsRecoveryInput() = failedRestart(input = true)

    @Test fun restartRetiresEveryOwnerWhenCancellationAndCleanupFail() {
        failedRestart(input = false, cancellation = true)
        failedRestart(input = true, cancellation = true)
    }

    @Test fun restartPreservesTheFirstErrorWhenSecretViewCleanupAlsoFails() {
        failedRestart(input = false, cancellation = true, clearing = true)
        failedRestart(input = true, clearing = true)
    }

    @Test fun successfulRestartClearsOldSetupAndReturnsToEmptyWalletChoices() = withFixture { fixture ->
        val welcome = CountDownLatch(1)
        try {
            onMain {
                fixture.handler.enqueue = true
                assertTrue(fixture.start())
                val (words, view) = shownSecret(fixture, input = false)
                try {
                    fixture.observeEmptyRestart(welcome)
                    fixture.restart()
                    assertTrue(words.all { it == '\u0000' })
                    assertEquals(View.INVISIBLE, view.visibility)
                    assertTrue(fixture.worker.isClosed)
                    assertNotNull(field("session").get(fixture.controller))
                    assertNotSame(fixture.session, field("session").get(fixture.controller))
                    assertNull(field("setupTimeout").get(fixture.controller))
                    assertFalse(fixture.handler.hasCallbacks(checkNotNull(fixture.handler.timeout)))
                } finally { words.fill('\u0000') }
            }
            assertTrue("Empty disposable storage did not return to wallet choices", welcome.await(5, TimeUnit.SECONDS))
            onMain {
                assertFalse(field("busy").getBoolean(fixture.controller))
                assertNotNull(fixture.host.findViewById<View>(R.id.restore_wallet))
            }
            fixture.finishWorker()
        } finally {
            onMain { fixture.destroy(); fixture.assertRetired() }
        }
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

    @Test fun destructionCancellationFailureStillRetiresEveryOwner() {
        for (problem in listOf(IllegalStateException("Public cancellation refusal"),
            OutOfMemoryError("Public cancellation error"))) withFixture { fixture ->
            val words = charArrayOf('a', 'b', 'c')
            try {
                onMain {
                    fixture.handler.enqueue = true
                    assertTrue(fixture.start())
                    fixture.screens.backup(words, {}, {})
                    val signal = fixture.failCancellation(problem)
                    assertSame(problem, assertThrows(Throwable::class.java) { fixture.destroy() })
                    assertTrue(signal.isCanceled)
                    assertFalse(fixture.authentication.hasPending)
                    fixture.assertRetired()
                    assertFalse(field("resumed").getBoolean(fixture.controller))
                    assertTrue(field("busy").getBoolean(fixture.controller))
                    assertTrue(words.all { it == '\u0000' })
                    assertEquals(1, (fixture.controller.application as LifecycleApplication).destructions)
                    assertEquals(View.INVISIBLE,
                        fixture.host.findViewById<View>(R.id.recovery_words).visibility)
                    // An active worker still owns its marker until termination.
                    assertTrue(fixture.entropy.all { it == 0x61.toByte() })
                    fixture.destroy()
                    assertEquals(2, (fixture.controller.application as LifecycleApplication).destructions)
                }
                fixture.finishWorker()
            } finally { words.fill('\u0000') }
        }
    }

    @Test fun destructionRetiresWorkerAndTimerBeforeAViewClearFailure() = destructionViewFailure(false)

    @Test fun destructionPreservesCancellationErrorWhenViewClearingAlsoFails() = destructionViewFailure(true)

    private fun destructionViewFailure(failCancellation: Boolean) = withFixture { fixture ->
        val words = charArrayOf('a', 'b', 'c')
        val problem = OutOfMemoryError("Public destruction display failure")
        onMain {
            fixture.handler.enqueue = true
            assertTrue(fixture.start())
            fixture.screens.backup(words, {}, {})
            val cancellation = OutOfMemoryError("Public primary cancellation error")
            if (failCancellation) fixture.failCancellation(cancellation)
            val display = fixture.host.findViewById<RecoveryWordsView>(R.id.recovery_words)
            val listener = object : TextWatcher {
                override fun beforeTextChanged(text: CharSequence?, start: Int, count: Int, after: Int) {
                    if (after == 0) { fixture.assertRetired(); throw problem }
                }
                override fun onTextChanged(text: CharSequence?, start: Int, before: Int, count: Int) = Unit
                override fun afterTextChanged(text: Editable?) = Unit
            }
            display.addTextChangedListener(listener)
            try {
                assertSame(if (failCancellation) cancellation else problem,
                    assertThrows(OutOfMemoryError::class.java) { fixture.destroy() })
                assertTrue(words.all { it == '\u0000' })
                assertEquals(View.INVISIBLE, display.visibility)
                assertEquals(1, (fixture.controller.application as LifecycleApplication).destructions)
            } finally { display.removeTextChangedListener(listener); words.fill('\u0000') }
            fixture.destroy()
        }
        fixture.finishWorker()
    }

    @Test fun destructionToleratesIncompleteCreation() = onMain {
        val controller = controller() // Never launched or given storage.
        try {
            MainActivity::class.java.getDeclaredMethod("onDestroy").apply { isAccessible = true }
                .invoke(controller)
        } catch (wrapped: InvocationTargetException) { throw checkNotNull(wrapped.cause) }
        assertNull(field("session").get(controller))
        assertNull(field("setupTimeout").get(controller))
        assertEquals(1, (controller.application as LifecycleApplication).destructions)
    }
}
