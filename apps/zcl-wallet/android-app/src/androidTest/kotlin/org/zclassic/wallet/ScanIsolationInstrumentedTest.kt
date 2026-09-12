// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.os.Process
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import com.google.zxing.BarcodeFormat
import com.google.zxing.qrcode.QRCodeWriter
import java.nio.ByteBuffer
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicReference
import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith
import org.zclassic.wallet.core.CameraFrames
import org.zclassic.wallet.core.Network
import org.zclassic.wallet.core.PaymentRequest
import org.zclassic.wallet.core.Zatoshi

@RunWith(AndroidJUnit4::class)
class ScanIsolationInstrumentedTest {
    private val instrumentation = InstrumentationRegistry.getInstrumentation()

    @Test fun isolatedBinderIdentityAndRequestRoundTrip() {
        val ready = CountDownLatch(1)
        val failed = AtomicBoolean()
        val client = ScanDecodeClient(instrumentation.targetContext,
            { ready.countDown() }, { failed.set(true); ready.countDown() })
        try {
            instrumentation.runOnMainSync { client.connect() }
            assertTrue("Decoder startup timed out", ready.await(20, TimeUnit.SECONDS))
            assertFalse("Decoder connection refused", failed.get())
            assertTrue(client.isReady)
            assertNotEquals(Process.myUid(), client.decoderUid)
            val address = "t1T8yaLVhNqxA5KJcmiqqFN88e8DNp2PBfF"
            val matrix = QRCodeWriter().encode("zclassic:$address?amount=1.25&label=Fixture",
                BarcodeFormat.QR_CODE, 333, 333)
            val plane = ByteBuffer.allocateDirect(333 * 333)
            for (i in 0 until plane.capacity()) plane.put(if (matrix[i % 333, i / 333]) 0 else -1)
            plane.flip()
            val original = requireNotNull(CameraFrames.pack(plane, 333, 333, 333, 1))
            for (network in listOf(Network.MAINNET, Network.TESTNET)) {
                val reply = CountDownLatch(1)
                val result = AtomicReference<PaymentRequest?>()
                val frame = original.copyOf()
                instrumentation.runOnMainSync {
                    assertTrue(client.submit(frame, network) { result.set(it); reply.countDown() })
                }
                assertTrue(frame.all { it == 0.toByte() })
                assertTrue("Decoder reply timed out", reply.await(10, TimeUnit.SECONDS))
                assertFalse(failed.get())
                if (network == Network.MAINNET) {
                    assertEquals(address, result.get()?.address?.encoded)
                    assertEquals(Zatoshi.of(125000000), result.get()?.amount)
                    assertEquals("Fixture", result.get()?.label)
                } else assertNull(result.get())
            }
            instrumentation.runOnMainSync { client.close() }
            assertFalse(client.isReady)
            val refused = original.copyOf()
            assertFalse(client.submit(refused, Network.MAINNET) { fail("Closed client delivered") })
            assertTrue(refused.all { it == 0.toByte() })
            original.fill(0)
        } finally {
            instrumentation.runOnMainSync { client.close() }
        }
    }
}
