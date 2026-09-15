// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.app.Activity
import android.os.CancellationSignal
import android.os.Handler
import android.os.Looper
import android.os.Message
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import java.util.concurrent.atomic.AtomicReference
import javax.crypto.Cipher
import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith
import org.zclassic.wallet.core.Network

/** Unattached Activity, uninitialized public cipher handle and injected main
 * queue failures. No prompt, authentication, Keystore key or wallet is opened. */
@RunWith(AndroidJUnit4::class)
class AuthenticationSetupFailureInstrumentedTest {
    private val instrumentation = InstrumentationRegistry.getInstrumentation()

    private fun field(name: String) = WalletAuthentication::class.java.getDeclaredField(name).apply { isAccessible = true }

    private fun onMain(action: () -> Unit) {
        val failure = AtomicReference<Throwable?>()
        instrumentation.runOnMainSync {
            try { action() } catch (problem: Throwable) { failure.set(problem) }
        }
        failure.get()?.let { throw it }
    }

    private class PostingHandler(private val enqueue: Boolean, private val problem: Throwable?,
                                 private val observe: () -> Unit) : Handler(Looper.getMainLooper()) {
        var timeout: Runnable? = null
        override fun sendMessageAtTime(message: Message, uptimeMillis: Long): Boolean {
            timeout = message.callback
            observe()
            val accepted = enqueue && super.sendMessageAtTime(message, uptimeMillis)
            check(!enqueue || accepted)
            problem?.let { throw it }
            return accepted
        }
    }

    /** Created and used on the main thread, as required by the actual owner. */
    private inner class Fixture(enqueue: Boolean, problem: Throwable?, callbackFailure: Throwable? = null) : AutoCloseable {
        var approved = 0
        var failed = 0
        var signal: CancellationSignal? = null
        val auth = WalletAuthentication(Activity(), { approved++ }, {
            failed++
            callbackFailure?.let { throw it }
        })
        val handler = PostingHandler(enqueue, problem) {
            val pending = field("pending").get(auth)
            assertNotNull(pending)
            val signalField = pending.javaClass.getDeclaredField("signal").apply { isAccessible = true }
            signal = signalField.get(pending) as CancellationSignal
        }
        private val prepared = PreparedWalletAction(WalletAction.CREATE,
            Cipher.getInstance("AES/GCM/NoPadding"), Network.MAINNET)

        init { field("handler").set(auth, handler) }

        fun begin(): Throwable? = try { auth.begin(prepared); null } catch (problem: Throwable) { problem }

        fun assertCleared() {
            assertNull("Scheduling failure retained authentication request", field("pending").get(auth))
            assertNotNull(signal)
            assertTrue(signal!!.isCanceled)
            assertNotNull(handler.timeout)
            assertFalse(handler.hasCallbacks(handler.timeout!!))
            assertEquals(0, approved)
        }

        override fun close() { auth.cancel(); handler.removeCallbacksAndMessages(null) }
    }

    private fun failedSchedule(enqueue: Boolean, fatal: Boolean, secondary: Boolean = false) = onMain {
        val problem = if (fatal) OutOfMemoryError("Public injected authentication timer failure")
            else IllegalStateException("Public injected authentication timer refusal")
        val callback = if (secondary) OutOfMemoryError("Public injected notification failure") else null
        Fixture(enqueue, problem, callback).use { fixture ->
            fixture.auth.onResume()
            val observed = fixture.begin()
            fixture.assertCleared()
            assertFalse(fixture.auth.hasPending)
            assertEquals(1, fixture.failed)
            if (fatal) assertSame(problem, observed) else assertNull(observed)
            fixture.auth.onResume()
            assertEquals(1, fixture.failed)
        }
    }

    @Test fun schedulingExceptionClearsBeforeAndAfterEnqueue() {
        failedSchedule(enqueue = false, fatal = false)
        failedSchedule(enqueue = true, fatal = false)
    }

    @Test fun schedulingErrorClearsBeforeAndAfterEnqueue() {
        failedSchedule(enqueue = false, fatal = true)
        failedSchedule(enqueue = true, fatal = true)
    }

    @Test fun secondaryNotificationErrorCannotReplaceTheOriginalError() =
        failedSchedule(enqueue = true, fatal = true, secondary = true)

    @Test fun falsePostRefusesWithoutAnyPrompt() = onMain {
        Fixture(enqueue = false, problem = null).use { fixture ->
            fixture.auth.onResume()
            assertNull(fixture.begin())
            fixture.assertCleared()
            assertFalse(fixture.auth.hasPending)
            assertEquals(1, fixture.failed)
        }
    }

    @Test fun falsePostPreservesNotificationException() = onMain {
        val problem = IllegalStateException("Public injected notification refusal")
        Fixture(enqueue = false, problem = null, callbackFailure = problem).use { fixture ->
            fixture.auth.onResume()
            assertSame(problem, fixture.begin())
            fixture.assertCleared()
            assertFalse(fixture.auth.hasPending)
            assertEquals(1, fixture.failed)
        }
    }

    @Test fun backgroundSchedulingFailureDefersOnlyTheNotification() = onMain {
        Fixture(enqueue = true, problem = IllegalStateException("Public background refusal")).use { fixture ->
            val observed = fixture.begin()
            fixture.assertCleared()
            assertNull(observed)
            assertTrue(fixture.auth.hasPending)
            assertEquals(0, fixture.failed)
            fixture.auth.onResume()
            assertFalse(fixture.auth.hasPending)
            assertEquals(1, fixture.failed)
            fixture.auth.onResume()
            assertEquals(1, fixture.failed)
        }
    }
}
