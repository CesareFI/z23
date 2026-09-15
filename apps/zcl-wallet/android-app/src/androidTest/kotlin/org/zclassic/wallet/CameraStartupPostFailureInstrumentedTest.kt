// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.os.Handler
import android.os.HandlerThread
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

/** Starts a real worker, then refuses the startup timeout before any camera
 * open can be posted. No camera, permission, wallet or actual OOM is involved. */
@RunWith(AndroidJUnit4::class)
class CameraStartupPostFailureInstrumentedTest {
    private val instrumentation = InstrumentationRegistry.getInstrumentation()

    private fun field(name: String) = CameraCapture::class.java.getDeclaredField(name).apply { isAccessible = true }

    private fun onMain(action: () -> Unit) {
        val failure = AtomicReference<Throwable?>()
        instrumentation.runOnMainSync {
            try { action() } catch (problem: Throwable) { failure.set(problem) }
        }
        failure.get()?.let { throw it }
    }

    private class PostingHandler(private val enqueue: Boolean, private val problem: Throwable?) : Handler(Looper.getMainLooper()) {
        var timeout: Runnable? = null
        private var posts = 0
        override fun sendMessageAtTime(message: Message, uptimeMillis: Long): Boolean {
            if (++posts == 1) {
                timeout = message.callback
                val accepted = enqueue && super.sendMessageAtTime(message, uptimeMillis)
                check(!enqueue || accepted)
                problem?.let { throw it }
                return accepted
            }
            return super.sendMessageAtTime(message, uptimeMillis)
        }
    }

    private fun assertNewAdmission() {
        val entries = AtomicInteger()
        val problem = OutOfMemoryError("Public fresh-owner probe")
        val fresh = CameraCapture(instrumentation.targetContext,
            { _, _, _, _ -> error("No frame is possible") }, { error("Fatal setup cannot notify") }, {
                entries.incrementAndGet()
                throw problem // Prove admission without starting another worker.
            })
        onMain {
            assertSame(problem, assertThrows(OutOfMemoryError::class.java) { fresh.start() })
            assertEquals(1, entries.get())
            assertFalse((field("cameraOwner").get(null) as AtomicBoolean).get())
            fresh.close()
        }
    }

    private fun failedTimeout(enqueue: Boolean, problem: Throwable?) {
        assertFalse("Fixture requires no existing camera owner",
            (field("cameraOwner").get(null) as AtomicBoolean).get())
        val created = AtomicInteger()
        val frames = AtomicInteger()
        val reported = AtomicInteger()
        val worker = HandlerThread("WalletCamera")
        val main = PostingHandler(enqueue, problem)
        val camera = CameraCapture(instrumentation.targetContext,
            { _, _, _, _ -> frames.incrementAndGet() }, { reported.incrementAndGet() }, {
                created.incrementAndGet()
                worker
            })
        field("main").set(camera, main)
        try {
            onMain {
                if (problem is Error) assertSame(problem, assertThrows(Error::class.java) { camera.start() })
                else camera.start()
                assertEquals(1, created.get())
                assertNotNull("A real worker Handler must have been published", field("handler").get(camera))
                assertTrue("Failed startup retained an open lifetime",
                    (field("closed").get(camera) as AtomicBoolean).get())
                assertNotNull(main.timeout)
                assertFalse("Failed startup retained its timeout", main.hasCallbacks(main.timeout!!))
                camera.start()
                assertEquals(1, created.get())
            }
            worker.join(5_000)
            assertFalse("Failed startup retained a live worker", worker.isAlive)
            assertTrue((field("released").get(camera) as AtomicBoolean).get())
            assertFalse((field("ownsCamera").get(camera) as AtomicBoolean).get())
            assertFalse((field("cameraOwner").get(null) as AtomicBoolean).get())
            instrumentation.waitForIdleSync()
            assertEquals(0, frames.get())
            assertEquals(if (problem is Error) 0 else 1, reported.get())
            assertNewAdmission()
        } finally {
            // Normal owner cleanup also retires the deliberately failing old
            // implementation. Do not reset global admission or quit its worker
            // outside the owner: either would hide a shutdown defect.
            onMain { camera.close(); main.removeCallbacksAndMessages(null) }
            worker.join(5_000)
            assertFalse("Fixture owner cleanup failed to stop its worker", worker.isAlive)
        }
    }

    @Test fun errorBeforeTimeoutEnqueueClosesThePublishedWorker() =
        failedTimeout(enqueue = false, problem = OutOfMemoryError("Public timer allocation failure"))

    @Test fun errorAfterTimeoutEnqueueRemovesItAndClosesThePublishedWorker() =
        failedTimeout(enqueue = true, problem = OutOfMemoryError("Public timer handoff failure"))

    @Test fun ordinaryTimeoutRefusalsStillCloseAndReportOnce() {
        failedTimeout(enqueue = false, problem = null)
        failedTimeout(enqueue = false, problem = IllegalStateException("Public timer refusal"))
        failedTimeout(enqueue = true, problem = IllegalStateException("Public timer handoff refusal"))
    }
}
