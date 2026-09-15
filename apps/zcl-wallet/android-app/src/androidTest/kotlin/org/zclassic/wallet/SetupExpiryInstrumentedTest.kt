// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.widget.TextView
import androidx.test.core.app.ActivityScenario
import androidx.test.ext.junit.runners.AndroidJUnit4
import java.io.File
import java.nio.file.Files
import java.util.concurrent.ConcurrentLinkedQueue
import java.util.concurrent.Executor
import java.util.concurrent.ThreadPoolExecutor
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicLong
import java.util.concurrent.atomic.AtomicReference
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.zclassic.wallet.core.WalletStorage

/** Real views and controller routing on the storage-free debug host. The
 * controller is never attached/launched: no onCreate, storage, prompt or key.
 * Only a public injected clock advances; device time is never changed. */
@RunWith(AndroidJUnit4::class)
class SetupExpiryInstrumentedTest {
    private fun set(controller: MainActivity, name: String, value: Any) {
        MainActivity::class.java.getDeclaredField(name).apply { isAccessible = true }.set(controller, value)
    }

    private fun enter(controller: MainActivity, retry: Boolean = false) {
        MainActivity::class.java.getDeclaredMethod("enterRecovery", Boolean::class.javaPrimitiveType,
            Boolean::class.javaPrimitiveType).apply { isAccessible = true }.invoke(controller, false, retry)
    }

    private fun withController(action: (WalletDisplayFixtureActivity, MainActivity, AtomicLong) -> Unit) {
        ActivityScenario.launch(WalletDisplayFixtureActivity::class.java).use { scenario ->
            val failure = AtomicReference<Throwable>()
            scenario.onActivity { host ->
                val screens = WalletScreens(host)
                val controller = MainActivity()
                val clock = AtomicLong(100)
                val window = SetupWindow(clock::get)
                set(controller, "screens", screens)
                set(controller, "resumed", true)
                set(controller, "busy", false)
                set(controller, "setupWindow", window)
                try { action(host, controller, clock) }
                catch (problem: Throwable) { failure.set(problem) }
                finally { screens.clearSecrets() }
            }
            failure.get()?.let { throw it }
        }
    }

    @Test fun delayedRecoveryEntryRefusesAtTheExactSetupDeadline() = withController { host, controller, clock ->
        clock.set(600_100)
        enter(controller)
        assertNull("Expired setup displayed a recovery keyboard", host.findViewById<RecoveryInputView>(R.id.recovery_input))
        assertEquals(host.getString(R.string.setup_expired), host.findViewById<TextView>(R.id.status_message).text.toString())
    }

    @Test fun retryCannotRestartTheWindowOrRetainOldInput() = withController { host, controller, clock ->
        clock.set(600_099)
        enter(controller)
        val input = host.findViewById<RecoveryInputView>(R.id.recovery_input)
        assertNotNull(input)
        input.append('a') // Public marker, never a valid recovery phrase.
        clock.set(600_100)
        enter(controller, retry = true)
        assertNull("Expired retry restarted recovery entry", host.findViewById<RecoveryInputView>(R.id.recovery_input))
        assertEquals(0, input.takeInput().size)
        assertEquals(host.getString(R.string.setup_expired), host.findViewById<TextView>(R.id.status_message).text.toString())
    }

    @Test fun expiredSubmitClosesTheSessionBeforeQueuingPhraseWork() = withController { host, controller, clock ->
        val parent = Files.createTempDirectory(host.cacheDir.toPath(), "public-setup-submit-").toFile()
        val callbacks = ConcurrentLinkedQueue<Runnable>()
        val session = WalletPlatformSession(host, WalletStorage(File(parent, "untouched").absolutePath),
            Executor { callbacks.add(it) })
        val work = WalletPlatformSession::class.java.getDeclaredField("work").apply { isAccessible = true }
            .get(session) as OwnedExecutor
        set(controller, "session", session)
        try {
            enter(controller)
            val input = host.findViewById<RecoveryInputView>(R.id.recovery_input)
            input.append('a')
            clock.set(600_100)
            assertTrue(host.findViewById<android.view.View>(R.id.save_wallet).performClick())
            assertTrue("Expired input reached the platform queue", work.isClosed)
            assertTrue(callbacks.isEmpty())
            assertEquals(0, input.takeInput().size)
            assertEquals(host.getString(R.string.setup_expired), host.findViewById<TextView>(R.id.status_message).text.toString())
        } finally {
            session.close()
            val backend = OwnedExecutor::class.java.getDeclaredField("executor").apply { isAccessible = true }
                .get(work) as ThreadPoolExecutor?
            if (backend != null) assertTrue(backend.awaitTermination(5, TimeUnit.SECONDS))
            assertTrue("Public expiry fixture unexpectedly created files", parent.delete())
        }
    }
}
