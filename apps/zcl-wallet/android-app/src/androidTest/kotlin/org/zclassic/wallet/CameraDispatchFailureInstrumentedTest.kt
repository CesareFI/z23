// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.os.Handler
import android.os.Looper
import android.os.Message
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import java.lang.reflect.InvocationTargetException
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicInteger
import java.util.concurrent.atomic.AtomicReference
import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith

/** Public pixel packets and a private Handler only. No camera is started and
 * no actual allocation pressure, permission, wallet or key is involved. */
@RunWith(AndroidJUnit4::class)
class CameraDispatchFailureInstrumentedTest {
    private val instrumentation = InstrumentationRegistry.getInstrumentation()

    private class PostingHandler(private val enqueue: Boolean, private val problem: Throwable?) :
        Handler(Looper.getMainLooper()) {
        var posts = 0
        override fun sendMessageAtTime(message: Message, uptimeMillis: Long): Boolean {
            ++posts
            val accepted = enqueue && super.sendMessageAtTime(message, uptimeMillis)
            check(!enqueue || accepted) { "Fixture main queue refused its control post" }
            problem?.let { throw it }
            return accepted
        }
    }

    private fun field(name: String) = CameraCapture::class.java.getDeclaredField(name).apply {
        isAccessible = true
    }

    private fun onMain(action: () -> Unit) {
        val failure = AtomicReference<Throwable?>()
        instrumentation.runOnMainSync {
            try { action() } catch (problem: Throwable) { failure.set(problem) }
        }
        failure.get()?.let { throw it }
    }

    private fun packet() = ByteArray(5 + 21 * 21) { 93 }.apply {
        this[0] = 1; this[1] = 21; this[2] = 0; this[3] = 21; this[4] = 0
    }

    private fun dispatch(camera: CameraCapture, packet: ByteArray) {
        (field("framePending").get(camera) as AtomicBoolean).set(true)
        CameraCapture::class.java.getDeclaredMethod("dispatch", ByteArray::class.java).apply {
            isAccessible = true
        }.invoke(camera, packet)
    }

    private fun assertRetired(camera: CameraCapture, packet: ByteArray) {
        assertTrue("Failed camera handoff retained pixel bytes", packet.all { it == 0.toByte() })
        assertNull((field("queuedPacket").get(camera) as AtomicReference<*>).get())
        assertFalse((field("framePending").get(camera) as AtomicBoolean).get())
    }

    private fun refused(enqueue: Boolean, fatal: Boolean) {
        val delivered = AtomicInteger()
        val reported = AtomicInteger()
        val problem = if (fatal) OutOfMemoryError("Synthetic public camera post failure")
            else IllegalStateException("Synthetic public camera post refusal")
        val handler = PostingHandler(enqueue, problem)
        val input = packet()
        val camera = CameraCapture(instrumentation.targetContext,
            { _, _, _, _ -> delivered.incrementAndGet() }, { reported.incrementAndGet() })
        try {
            onMain {
                field("main").set(camera, handler)
                val thrown = assertThrows(InvocationTargetException::class.java) { dispatch(camera, input) }.cause
                assertSame("The original failure must propagate", problem, thrown)
                assertEquals(1, handler.posts)
                assertRetired(camera, input)
            }
            // A post that enqueued before throwing must leave its queued
            // callback inert; do not remove it to make this assertion pass.
            instrumentation.waitForIdleSync()
            assertEquals(0, delivered.get())
            assertEquals(0, reported.get())
        } finally {
            onMain { camera.close(); handler.removeCallbacksAndMessages(null) }
            input.fill(0)
        }
    }

    @Test fun throwingPostErasesAndRetiresTheOwnedFrame() {
        refused(enqueue = false, fatal = true)
        refused(enqueue = false, fatal = false)
    }

    @Test fun throwingAfterEnqueueCannotDeliverRetiredPixels() {
        refused(enqueue = true, fatal = true)
        refused(enqueue = true, fatal = false)
    }

    @Test fun refusedPostAndClosedCaptureClearWithoutDelivery() {
        val delivered = AtomicInteger()
        for (closed in listOf(false, true)) {
            val handler = PostingHandler(enqueue = false, problem = null)
            val input = packet()
            val camera = CameraCapture(instrumentation.targetContext,
                { _, _, _, _ -> delivered.incrementAndGet() }, {})
            try {
                onMain {
                    field("main").set(camera, handler)
                    if (closed) camera.close()
                    dispatch(camera, input)
                    assertRetired(camera, input)
                    assertTrue((field("closed").get(camera) as AtomicBoolean).get())
                    if (closed) assertEquals(0, handler.posts)
                }
                instrumentation.waitForIdleSync()
                assertEquals(0, delivered.get())
            } finally {
                onMain { camera.close(); handler.removeCallbacksAndMessages(null) }
                input.fill(0)
            }
        }
    }

    @Test fun acceptedPostTransfersOnceAndClearsAfterTheReceiver() {
        val delivered = AtomicInteger()
        val callbackFailure = AtomicReference<Throwable?>()
        val handler = PostingHandler(enqueue = true, problem = null)
        val input = packet()
        val before = input.copyOf()
        val camera = CameraCapture(instrumentation.targetContext, { owner, bytes, _, _ ->
            try {
                assertSame(input, bytes)
                assertArrayEquals(before, bytes)
                delivered.incrementAndGet()
            } catch (problem: Throwable) { callbackFailure.set(problem) }
            finally { owner.frameDone() }
        }, {})
        try {
            onMain {
                field("main").set(camera, handler)
                dispatch(camera, input)
                assertArrayEquals("Accepted delivery must retain its input until claim", before, input)
            }
            instrumentation.waitForIdleSync()
            callbackFailure.get()?.let { throw it }
            assertEquals(1, delivered.get())
            assertEquals(1, handler.posts)
            onMain { assertRetired(camera, input) }
        } finally {
            onMain { camera.close(); handler.removeCallbacksAndMessages(null) }
            input.fill(0)
            before.fill(0)
        }
    }
}
