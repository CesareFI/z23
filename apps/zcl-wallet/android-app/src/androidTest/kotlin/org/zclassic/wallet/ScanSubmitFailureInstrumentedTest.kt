// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.os.Handler
import android.os.Looper
import android.os.Message
import android.os.Process
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicInteger
import java.util.concurrent.atomic.AtomicReference
import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith
import org.zclassic.wallet.core.Network
import org.zclassic.wallet.scanipc.IScanDecoder
import org.zclassic.wallet.scanipc.IScanReply

/** Local Binder stub and public synthetic bytes; no camera, service binding,
 * wallet or memory exhaustion. Real Binder isolation has a separate fixture. */
@RunWith(AndroidJUnit4::class)
class ScanSubmitFailureInstrumentedTest {
    private val instrumentation = InstrumentationRegistry.getInstrumentation()

    private class PostingHandler(private val enqueue: Boolean, private val first: Throwable?,
                                 private val second: Throwable?) : Handler(Looper.getMainLooper()) {
        var timeout: Runnable? = null
        private var posts = 0
        override fun sendMessageAtTime(message: Message, uptimeMillis: Long): Boolean {
            ++posts
            if (posts == 1) {
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

    private fun field(name: String) = ScanDecodeClient::class.java.getDeclaredField(name).apply { isAccessible = true }

    private fun onMain(action: () -> Unit) {
        val failure = AtomicReference<Throwable?>()
        instrumentation.runOnMainSync {
            try { action() } catch (problem: Throwable) { failure.set(problem) }
        }
        failure.get()?.let { throw it }
    }

    private inner class Fixture(enqueue: Boolean = true, postFailure: Throwable? = null,
                               notificationFailure: Throwable? = null,
                               decodeFailure: Throwable? = null) : AutoCloseable {
        val handler = PostingHandler(enqueue, postFailure, notificationFailure)
        val reported = AtomicInteger()
        val calls = AtomicInteger()
        val frame = ByteArray(446) { 42 } // Public bytes; never sent to a decoder.
        val client = ScanDecodeClient(instrumentation.targetContext,
            { error("No connection is requested") }, { reported.incrementAndGet() }, { 100 })
        private val endpoint = object : IScanDecoder.Stub() {
            override fun ready(reply: IScanReply?) = error("No connection is requested")
            override fun decode(frame: ByteArray?, network: Int, requestId: Long, reply: IScanReply?) {
                calls.incrementAndGet()
                assertNotNull(frame)
                assertTrue(frame!!.size == 446 && frame.all { it == 42.toByte() })
                assertEquals(0, network)
                assertTrue(requestId > 0)
                assertNotNull(reply)
                decodeFailure?.let { throw it }
            }
        }

        init {
            field("main").set(client, handler)
            field("service").set(client, AtomicReference<IScanDecoder?>(endpoint))
            (field("serviceUid").get(client) as AtomicInteger).set(Process.myUid())
            assertTrue(client.isReady)
        }

        fun submit() = client.submit(frame, Network.MAINNET) { error("No reply is sent") }

        fun assertRetired() {
            assertTrue(frame.all { it == 0.toByte() })
            assertNull((field("pending").get(client) as AtomicReference<*>).get())
            assertNull((field("service").get(client) as AtomicReference<*>).get())
            assertTrue((field("closed").get(client) as AtomicBoolean).get())
            assertFalse(client.isReady)
            assertNotNull(handler.timeout)
            assertFalse(handler.hasCallbacks(handler.timeout!!))
        }

        fun checkNotifications(expected: Int) {
            instrumentation.waitForIdleSync()
            assertEquals(expected, reported.get())
        }

        override fun close() {
            onMain { client.close(); handler.removeCallbacksAndMessages(null) }
            frame.fill(0)
        }
    }

    private fun failedPost(enqueue: Boolean, secondary: Boolean = false) {
        val problem = OutOfMemoryError("Public injected timeout post failure")
        val notification = if (secondary) OutOfMemoryError("Public injected notification failure") else null
        Fixture(enqueue, problem, notification).use { fixture ->
            onMain {
                assertSame(problem, assertThrows(Throwable::class.java) { fixture.submit() })
                fixture.assertRetired()
                assertEquals(0, fixture.calls.get())
            }
            fixture.checkNotifications(if (secondary) 0 else 1)
        }
    }

    @Test fun errorBeforeTimeoutEnqueueRetiresTheRequest() = failedPost(enqueue = false)
    @Test fun errorAfterTimeoutEnqueueRemovesTheTimeout() = failedPost(enqueue = true)
    @Test fun notificationFailureCannotReplaceTheOriginalError() = failedPost(enqueue = true, secondary = true)

    @Test fun decoderErrorRetiresTheAlreadyScheduledRequest() {
        val problem = OutOfMemoryError("Public injected Binder marshalling failure")
        Fixture(decodeFailure = problem).use { fixture ->
            onMain {
                assertSame(problem, assertThrows(Throwable::class.java) { fixture.submit() })
                fixture.assertRetired()
                assertEquals(1, fixture.calls.get())
            }
            fixture.checkNotifications(1)
        }
    }

    @Test fun ordinaryPostRefusalsStillReturnFalseAndClear() {
        for (problem in listOf(null, IllegalStateException("Public injected post refusal"))) {
            Fixture(enqueue = false, postFailure = problem).use { fixture ->
                onMain { assertFalse(fixture.submit()); fixture.assertRetired() }
                assertEquals(0, fixture.calls.get())
                fixture.checkNotifications(1)
            }
        }
    }

    @Test fun successfulSubmissionKeepsOneDeadlineAndConsumesCompetingFrame() {
        Fixture().use { fixture ->
            onMain {
                assertTrue(fixture.submit())
                assertTrue(fixture.frame.all { it == 0.toByte() })
                assertTrue(fixture.client.isReady)
                assertNotNull((field("pending").get(fixture.client) as AtomicReference<*>).get())
                assertNotNull(fixture.handler.timeout)
                assertTrue(fixture.handler.hasCallbacks(fixture.handler.timeout!!))
                val competitor = ByteArray(446) { 43 }
                assertFalse(fixture.client.submit(competitor, Network.MAINNET) { error("No reply") })
                assertTrue(competitor.all { it == 0.toByte() })
                assertEquals(1, fixture.calls.get())
            }
            fixture.checkNotifications(0)
        }
    }
}
