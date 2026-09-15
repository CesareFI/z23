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
import org.zclassic.wallet.core.PaymentRequest
import org.zclassic.wallet.scanipc.IScanReply

/** Public decoded text and a private main Handler. The pending request and
 * expected UID are installed only in this fixture; real Binder isolation is
 * separately covered by ScanIsolationInstrumentedTest. No service is bound. */
@RunWith(AndroidJUnit4::class)
class ScanReplyHandoffInstrumentedTest {
    private val instrumentation = InstrumentationRegistry.getInstrumentation()

    private class PostingHandler(private val enqueue: Boolean, private val first: Throwable?,
                                 private val second: Throwable? = null) : Handler(Looper.getMainLooper()) {
        var posts = 0
        override fun sendMessageAtTime(message: Message, uptimeMillis: Long): Boolean {
            ++posts
            if (posts == 1) {
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

    private inner class Fixture(val handler: PostingHandler) : AutoCloseable {
        val delivered = AtomicInteger()
        val reported = AtomicInteger()
        val client = ScanDecodeClient(instrumentation.targetContext,
            { error("No connection is requested") }, { reported.incrementAndGet() }, { 100 })
        val bytes = "zclassic:t1T8yaLVhNqxA5KJcmiqqFN88e8DNp2PBfF?amount=1.25".toByteArray(Charsets.US_ASCII)
        private val callbackFailure = AtomicReference<Throwable?>()
        val reply = field("reply").get(client) as IScanReply

        init {
            val receive: (PaymentRequest?) -> Unit = {
                try {
                    assertNotNull(it)
                    delivered.incrementAndGet()
                } catch (problem: Throwable) { callbackFailure.set(problem) }
            }
            val expire: (Long) -> Unit = { error("No timeout is scheduled by this fixture") }
            val type = ScanDecodeClient::class.java.declaredClasses.single { it.simpleName == "Pending" }
            val constructor = type.declaredConstructors.single { it.parameterCount == 5 }.apply { isAccessible = true }
            val request = constructor.newInstance(42L, Network.MAINNET, 100L, receive, expire)
            field("pending").set(client, AtomicReference(request))
            field("main").set(client, handler)
            (field("serviceUid").get(client) as AtomicInteger).set(Process.myUid())
        }

        fun assertRetired() {
            assertTrue("Failed reply handoff retained text", bytes.all { it == 0.toByte() })
            assertNull((field("pending").get(client) as AtomicReference<*>).get())
            assertTrue((field("closed").get(client) as AtomicBoolean).get())
        }

        fun checkCallbacks(deliveries: Int, failures: Int) {
            instrumentation.waitForIdleSync()
            callbackFailure.get()?.let { throw it }
            assertEquals(deliveries, delivered.get())
            assertEquals(failures, reported.get())
        }

        override fun close() {
            onMain { client.close(); handler.removeCallbacksAndMessages(null) }
            bytes.fill(0)
        }
    }

    private fun throwingPost(enqueue: Boolean, fatal: Boolean, failNotification: Boolean = false) {
        val problem = if (fatal) OutOfMemoryError("Public injected reply post failure")
            else IllegalStateException("Public injected reply post refusal")
        val secondary = if (failNotification) OutOfMemoryError("Public injected notification failure") else null
        Fixture(PostingHandler(enqueue, problem, secondary)).use { fixture ->
            onMain {
                val caught = assertThrows(Throwable::class.java) { fixture.reply.onResult(42, fixture.bytes) }
                assertSame("Original handoff failure must propagate", problem, caught)
                fixture.assertRetired()
            }
            // Do not remove a callback enqueued before the synthetic throw.
            // It must be inert after the request has been retired.
            fixture.checkCallbacks(0, if (failNotification) 0 else 1)
        }
    }

    @Test fun throwingPostClearsAndRetiresTheReply() {
        throwingPost(enqueue = false, fatal = false)
        throwingPost(enqueue = false, fatal = true)
    }

    @Test fun throwingAfterEnqueueCannotDeliverTheFailedReply() {
        throwingPost(enqueue = true, fatal = false)
        throwingPost(enqueue = true, fatal = true)
    }

    @Test fun secondaryNotificationFailurePreservesTheOriginalException() {
        throwingPost(enqueue = false, fatal = false, failNotification = true)
        throwingPost(enqueue = true, fatal = true, failNotification = true)
    }

    @Test fun falsePostClearsAndReportsOneFailure() {
        Fixture(PostingHandler(false, null)).use { fixture ->
            onMain { fixture.reply.onResult(42, fixture.bytes); fixture.assertRetired() }
            fixture.checkCallbacks(0, 1)
        }
    }

    @Test fun successfulPostDeliversOnceAndClearsItsText() {
        Fixture(PostingHandler(true, null)).use { fixture ->
            onMain { fixture.reply.onResult(42, fixture.bytes) }
            fixture.checkCallbacks(1, 0)
            assertTrue(fixture.bytes.all { it == 0.toByte() })
        }
    }
}
