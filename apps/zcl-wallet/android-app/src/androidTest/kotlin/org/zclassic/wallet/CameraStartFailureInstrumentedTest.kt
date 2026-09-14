// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.os.HandlerThread
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicInteger
import java.util.concurrent.atomic.AtomicReference
import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith

/** Synthetic construction/start failures only. No OS camera is opened, no
 * actual memory is exhausted and no permission, wallet or key is accessed. */
@RunWith(AndroidJUnit4::class)
class CameraStartFailureInstrumentedTest {
    private val instrumentation = InstrumentationRegistry.getInstrumentation()

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

    private fun verifyFailure(startFailure: Boolean, fatal: Boolean) {
        assertFalse("Fixture requires no current camera owner",
            (field("cameraOwner").get(null) as AtomicBoolean).get())
        val created = AtomicInteger()
        val reported = AtomicInteger()
        val problem = if (fatal) OutOfMemoryError("Synthetic public camera worker failure")
            else SecurityException("Synthetic public camera worker refusal")
        var worker: HandlerThread? = null
        val camera = CameraCapture(instrumentation.targetContext,
            { _, _, _, _ -> error("Failed worker delivered a frame") }, { reported.incrementAndGet() }, {
                created.incrementAndGet()
                if (!startFailure) throw problem
                object : HandlerThread("WalletCamera") {
                    override fun start() { throw problem }
                }.also { worker = it }
            })
        try {
            onMain {
                if (fatal) assertSame(problem, assertThrows(OutOfMemoryError::class.java) { camera.start() })
                else camera.start()
                assertEquals(1, created.get())
                assertNull(field("handler").get(camera))
                assertTrue((field("closed").get(camera) as AtomicBoolean).get())
                assertFalse((field("ownsCamera").get(camera) as AtomicBoolean).get())
                assertFalse((field("cameraOwner").get(null) as AtomicBoolean).get())
                assertTrue(worker?.isAlive != true)
                camera.start() // A failed lifetime cannot silently retry itself.
                assertEquals(1, created.get())
            }
            instrumentation.waitForIdleSync()
            assertEquals(if (fatal) 0 else 1, reported.get())
        } finally {
            // Recover only this fixture's pre-handler ownership on a failing
            // old implementation; its synthetic worker can never have started.
            onMain {
                camera.close()
                assertNull(field("handler").get(camera))
                assertTrue(worker?.isAlive != true)
                CameraCapture::class.java.getDeclaredMethod("releaseOwnership").apply {
                    isAccessible = true
                }.invoke(camera)
            }
        }
    }

    @Test fun allocationFailureBeforeWorkerConstructionReleasesTheCameraSlot() {
        verifyFailure(startFailure = false, fatal = true)
        verifyFailure(startFailure = false, fatal = false)
    }

    @Test fun workerStartFailureReleasesTheCameraSlotWithoutSwallowingFatalErrors() {
        verifyFailure(startFailure = true, fatal = true)
        verifyFailure(startFailure = true, fatal = false)
    }
}
