// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.Manifest
import android.content.pm.PackageManager
import android.os.Build
import android.os.Handler
import android.os.SystemClock
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicReference
import org.junit.Assert.*
import org.junit.Assume.assumeTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith

/** Holds delivery of real Camera2 callbacks on their existing worker. Reflection
 * only observes ownership and orders the existing release operation; no app
 * hook, fake camera, production timeout or permission policy is introduced. */
@RunWith(AndroidJUnit4::class)
class CameraOpenCancellationInstrumentedTest {
    private val instrumentation = InstrumentationRegistry.getInstrumentation()

    @Before fun emulatorOnly() {
        assumeTrue("Requires camera emulator fixture opt-in",
            InstrumentationRegistry.getArguments().getString("cameraFixture") == "yes")
        assertEquals("ranchu", Build.HARDWARE)
        assertEquals("org.zclassic.wallet.dev", instrumentation.targetContext.packageName)
        assertEquals(PackageManager.PERMISSION_GRANTED,
            instrumentation.targetContext.checkSelfPermission(Manifest.permission.CAMERA))
    }

    private fun field(name: String) = CameraCapture::class.java.getDeclaredField(name).apply {
        isAccessible = true
    }

    private fun cameraThreads() = Thread.getAllStackTraces().keys.count {
        it.name == "WalletCamera" && it.isAlive
    }

    private fun awaitClosed() {
        val deadline = SystemClock.uptimeMillis() + 45_000
        while (SystemClock.uptimeMillis() < deadline && cameraThreads() != 0) SystemClock.sleep(50)
        assertEquals("Camera cancellation retained a worker", 0, cameraThreads())
    }

    private class DeliveryGate {
        val reached = CountDownLatch(1)
        val cancel = CountDownLatch(1)
        val released = CountDownLatch(1)
        val resume = CountDownLatch(1)
        val pending = AtomicBoolean()
        val failure = AtomicReference<Throwable?>()

        fun waitFor(latch: CountDownLatch) {
            check(latch.await(30, TimeUnit.SECONDS)) { "Camera callback gate expired" }
        }
    }

    private fun holdDelivery(camera: CameraCapture, gate: DeliveryGate) {
        try {
            // This is the camera worker: opening has no cross-thread read/write.
            gate.pending.set(field("opening").getBoolean(camera))
            gate.reached.countDown()
            if (!gate.pending.get()) return
            gate.waitFor(gate.cancel)
            // Model close's queued release arriving before the terminal callback.
            // The real callback remains queued behind this bounded operation.
            CameraCapture::class.java.getDeclaredMethod("release").apply {
                isAccessible = true
            }.invoke(camera)
            gate.released.countDown()
            gate.waitFor(gate.resume)
        } catch (problem: Throwable) {
            gate.failure.set(problem)
            if (problem is InterruptedException) Thread.currentThread().interrupt()
        } finally {
            gate.reached.countDown()
            gate.released.countDown()
        }
    }

    private fun rejectCompetingOwner() {
        val failed = CountDownLatch(1)
        val frame = AtomicBoolean()
        val competitor = CameraCapture(instrumentation.targetContext,
            { owner, _, _, _ -> frame.set(true); owner.frameDone() }, { failed.countDown() })
        try {
            instrumentation.runOnMainSync { competitor.start() }
            assertTrue("Competing camera was not refused", failed.await(5, TimeUnit.SECONDS))
            assertFalse("Competing camera delivered a frame", frame.get())
            assertEquals("Pending open admitted another worker", 1, cameraThreads())
        } finally { instrumentation.runOnMainSync { competitor.close() } }
    }

    private fun cancelPendingDelivery(): Boolean {
        val gate = DeliveryGate()
        val frame = AtomicBoolean()
        val camera = CameraCapture(instrumentation.targetContext,
            { owner, _, _, _ -> frame.set(true); owner.frameDone() }, {})
        try {
            instrumentation.runOnMainSync {
                camera.start()
                val queue = field("handler").get(camera) as Handler
                assertTrue(queue.post { holdDelivery(camera, gate) })
            }
            assertTrue("Camera open never reached its delivery gate", gate.reached.await(20, TimeUnit.SECONDS))
            gate.failure.get()?.let { throw AssertionError("Camera callback gate failed", it) }
            // A terminal callback can win the initial scheduling race. Close this
            // attempt fully and retry; passing still requires a witnessed open.
            if (!gate.pending.get()) return false
            assertFalse("Camera expired before cancellation", (field("closed").get(camera) as AtomicBoolean).get())
            instrumentation.runOnMainSync { camera.close() }
            gate.cancel.countDown()
            assertTrue("Pending-open release was not observed", gate.released.await(20, TimeUnit.SECONDS))
            gate.failure.get()?.let { throw AssertionError("Pending-open release failed", it) }
            assertTrue("Pending open lost its owner", (field("ownsCamera").get(camera) as AtomicBoolean).get())
            assertFalse("Pending open released its resources", (field("released").get(camera) as AtomicBoolean).get())
            rejectCompetingOwner()
            assertFalse("Cancelled open delivered pixels", frame.get())
            return true
        } finally {
            gate.cancel.countDown()
            gate.resume.countDown()
            instrumentation.runOnMainSync { camera.close() }
            awaitClosed()
            gate.failure.get()?.let { throw AssertionError("Camera callback cleanup failed", it) }
            if (gate.pending.get()) assertFalse("Late callback delivered cancelled pixels", frame.get())
        }
    }

    private fun requireFreshCapture() {
        val delivered = CountDownLatch(1)
        val failed = AtomicBoolean()
        val camera = CameraCapture(instrumentation.targetContext,
            { owner, packet, _, _ ->
                assertTrue("Fresh camera delivered an empty packet", packet.isNotEmpty())
                owner.frameDone()
                owner.close()
                delivered.countDown()
            }, { failed.set(true) })
        try {
            instrumentation.runOnMainSync { camera.start() }
            assertTrue("Fresh camera did not recover after cancellation", delivered.await(25, TimeUnit.SECONDS))
            assertFalse("Fresh camera reported failure", failed.get())
        } finally {
            instrumentation.runOnMainSync { camera.close() }
            awaitClosed()
        }
    }

    @Test fun cancelledPendingOpenRetainsBoundedOwnerUntilRealCallback() {
        awaitClosed()
        var witnessed = false
        for (attempt in 0 until 5) {
            if (cancelPendingDelivery()) { witnessed = true; break }
        }
        assertTrue("No pending real camera-open callback was observed", witnessed)
        requireFreshCapture()
    }
}
