// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.os.Looper
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import com.google.zxing.BarcodeFormat
import com.google.zxing.qrcode.QRCodeWriter
import java.nio.ByteBuffer
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicInteger
import java.util.concurrent.atomic.AtomicLong
import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith
import org.zclassic.wallet.core.CameraFrames
import org.zclassic.wallet.core.Network

/** Public frames through the actual isolated service. The injected platform
 * clock models a stalled main queue without a five-second UI-thread sleep. */
@RunWith(AndroidJUnit4::class)
class ScanDeadlineInstrumentedTest {
    private val instrumentation = InstrumentationRegistry.getInstrumentation()

    private fun frame(): ByteArray {
        val matrix = QRCodeWriter().encode(
            "zclassic:t1T8yaLVhNqxA5KJcmiqqFN88e8DNp2PBfF?amount=1.25",
            BarcodeFormat.QR_CODE, 333, 333)
        val plane = ByteBuffer.allocateDirect(333 * 333)
        for (i in 0 until plane.capacity()) plane.put(if (matrix[i % 333, i / 333]) 0 else -1)
        plane.flip()
        return requireNotNull(CameraFrames.pack(plane, 333, 333, 333, 1))
    }

    private fun queuedReply(start: Long, deliveryTime: Long, expires: Boolean) {
        val clock = AtomicLong(start)
        val submitting = AtomicBoolean()
        val received = CountDownLatch(1)
        val ready = CountDownLatch(1)
        val finished = CountDownLatch(1)
        val failures = AtomicInteger()
        val deliveries = AtomicInteger()
        val client = ScanDecodeClient(instrumentation.targetContext,
            { ready.countDown() }, { failures.incrementAndGet(); ready.countDown(); finished.countDown() }) {
            val sampled = clock.get()
            if (submitting.get() && Thread.currentThread() !== Looper.getMainLooper().thread)
                received.countDown()
            sampled
        }
        try {
            instrumentation.runOnMainSync { client.connect() }
            assertTrue("Decoder startup timed out", ready.await(20, TimeUnit.SECONDS))
            assertEquals(0, failures.get())
            val packet = frame()
            instrumentation.runOnMainSync {
                submitting.set(true)
                assertTrue(client.submit(packet, Network.MAINNET) {
                    assertNotNull(it)
                    deliveries.incrementAndGet()
                    finished.countDown()
                })
                assertTrue(packet.all { it == 0.toByte() })
                // Binder observes the still-valid arrival time while delivery
                // remains queued behind this main-thread invocation.
                assertTrue("Isolated reply never arrived", received.await(4, TimeUnit.SECONDS))
                clock.set(deliveryTime)
            }
            assertTrue("No terminal callback", finished.await(10, TimeUnit.SECONDS))
            instrumentation.waitForIdleSync()
            assertEquals(if (expires) 1 else 0, failures.get())
            assertEquals(if (expires) 0 else 1, deliveries.get())
            assertEquals(!expires, client.isReady)
        } finally {
            instrumentation.runOnMainSync { client.close() }
        }
    }

    @Test fun queuedReplyExpiresAtDeadlineAndOnClockRollback() {
        queuedReply(100, 5_100, true)
        queuedReply(100, 99, true)
    }

    @Test fun queuedReplyBeforeDeadlineAndNearClockLimitStillDelivers() {
        queuedReply(100, 5_099, false)
        queuedReply(Long.MAX_VALUE - 1, Long.MAX_VALUE, false)
    }

    @Test fun queuedReadyExpiresBeforeCallback() {
        val clock = AtomicLong(100)
        val finished = CountDownLatch(1)
        val ready = AtomicInteger()
        val failed = AtomicInteger()
        val client = ScanDecodeClient(instrumentation.targetContext,
            { ready.incrementAndGet(); finished.countDown() },
            { failed.incrementAndGet(); finished.countDown() }) {
            val sampled = clock.get()
            if (Thread.currentThread() !== Looper.getMainLooper().thread) clock.set(15_100)
            sampled
        }
        try {
            instrumentation.runOnMainSync { client.connect() }
            assertTrue("No terminal connection callback", finished.await(20, TimeUnit.SECONDS))
            instrumentation.waitForIdleSync()
            assertEquals(0, ready.get())
            assertEquals(1, failed.get())
            assertFalse(client.isReady)
        } finally {
            instrumentation.runOnMainSync { client.close() }
        }
    }
}
