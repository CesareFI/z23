// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.content.Context
import android.content.ContextWrapper
import android.content.Intent
import android.content.ServiceConnection
import android.os.Handler
import android.os.Looper
import android.os.Message
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicInteger
import java.util.concurrent.atomic.AtomicReference
import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith

/** Public Handler fault injection before binding. No service, camera, wallet,
 * provider policy change or actual memory exhaustion is involved. */
@RunWith(AndroidJUnit4::class)
class ScanConnectFailureInstrumentedTest {
    private val instrumentation = InstrumentationRegistry.getInstrumentation()

    private class BindRecorder(context: Context) : ContextWrapper(context) {
        var calls = 0
        override fun getApplicationContext(): Context = this
        override fun bindService(intent: Intent, connection: ServiceConnection, flags: Int): Boolean {
            ++calls
            error("Refused setup must not reach service binding")
        }
    }

    private class PostingHandler(private val enqueue: Boolean, private val first: Throwable?,
                                 private val second: Throwable?) : Handler(Looper.getMainLooper()) {
        var timeout: Runnable? = null
        private var posts = 0
        override fun sendMessageAtTime(message: Message, uptimeMillis: Long): Boolean {
            if (++posts == 1) {
                timeout = message.callback
                val accepted = enqueue && super.sendMessageAtTime(message, uptimeMillis)
                check(!enqueue || accepted)
                first?.let { throw it }
                return accepted
            }
            second?.let { throw it }
            return super.sendMessageAtTime(message, uptimeMillis)
        }
    }

    private fun field(name: String) = ScanDecodeClient::class.java.getDeclaredField(name).apply {
        isAccessible = true
    }

    private fun onMain(action: () -> Unit) {
        val failure = AtomicReference<Throwable?>()
        instrumentation.runOnMainSync {
            try { action() } catch (problem: Throwable) { failure.set(problem) }
        }
        failure.get()?.let { throw it }
    }

    private inner class Fixture(enqueue: Boolean, first: Throwable?, second: Throwable? = null) : AutoCloseable {
        val context = BindRecorder(instrumentation.targetContext)
        val handler = PostingHandler(enqueue, first, second)
        val reported = AtomicInteger()
        val client = ScanDecodeClient(context, { error("No binding is requested") },
            { reported.incrementAndGet() }, { 100 })

        init { field("main").set(client, handler) }

        fun assertRetired() {
            assertTrue((field("closed").get(client) as AtomicBoolean).get())
            assertFalse((field("bound").get(client) as AtomicBoolean).get())
            assertNull((field("pending").get(client) as AtomicReference<*>).get())
            assertNull((field("service").get(client) as AtomicReference<*>).get())
            assertFalse(client.isReady)
            assertEquals(0, context.calls)
            assertSame(field("connectTimeout").get(client), handler.timeout)
            assertFalse(handler.hasCallbacks(checkNotNull(handler.timeout)))
            // Neither a stale wakeup nor an explicit retry revives this owner.
            checkNotNull(handler.timeout).run()
            client.connect()
            assertEquals(0, context.calls)
        }

        fun checkNotifications(expected: Int) {
            instrumentation.waitForIdleSync()
            assertEquals(expected, reported.get())
        }

        override fun close() = onMain {
            client.close()
            handler.removeCallbacksAndMessages(null)
        }
    }

    private fun failedPost(enqueue: Boolean, secondary: Boolean = false) {
        val first = OutOfMemoryError("Public injected connection timeout failure")
        val second = if (secondary) IllegalStateException("Public notification failure") else null
        Fixture(enqueue, first, second).use { fixture ->
            onMain {
                assertSame(first, assertThrows(Throwable::class.java) { fixture.client.connect() })
                fixture.assertRetired()
            }
            fixture.checkNotifications(if (secondary) 0 else 1)
        }
    }

    @Test fun errorBeforeTimeoutEnqueueRetiresConnectionSetup() = failedPost(enqueue = false)
    @Test fun errorAfterTimeoutEnqueueRemovesTheTimeout() = failedPost(enqueue = true)
    @Test fun notificationFailureCannotReplaceTheSetupError() = failedPost(enqueue = true, secondary = true)

    @Test fun ordinaryRefusalsStillRetireAndReportOnce() {
        for (first in listOf(null, IllegalStateException("Public timeout refusal"))) {
            Fixture(enqueue = false, first).use { fixture ->
                onMain { fixture.client.connect(); fixture.assertRetired() }
                fixture.checkNotifications(1)
            }
        }
    }

    @Test fun ordinaryExceptionAfterEnqueueStillRemovesTheTimeout() {
        Fixture(enqueue = true, IllegalStateException("Public timeout refusal")).use { fixture ->
            onMain { fixture.client.connect(); fixture.assertRetired() }
            fixture.checkNotifications(1)
        }
    }
}
