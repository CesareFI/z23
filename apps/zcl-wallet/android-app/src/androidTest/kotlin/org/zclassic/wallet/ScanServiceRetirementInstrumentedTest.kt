// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.content.Context
import android.content.ContextWrapper
import android.os.Build
import android.os.Process
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import com.google.zxing.BarcodeFormat
import com.google.zxing.qrcode.QRCodeWriter
import java.util.concurrent.CountDownLatch
import java.util.concurrent.ThreadPoolExecutor
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicInteger
import java.util.concurrent.atomic.AtomicReference
import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith
import org.zclassic.wallet.scanipc.IScanDecoder
import org.zclassic.wallet.scanipc.IScanReply

/** Local service endpoint, actual worker and packaged decoder JNI. A local
 * reply holds the handoff open so owned arrays remain observable. Real Binder
 * isolation is covered separately; no camera, permission or wallet is opened. */
@RunWith(AndroidJUnit4::class)
class ScanServiceRetirementInstrumentedTest {
    private val context = InstrumentationRegistry.getInstrumentation().targetContext
    private val uri = "zclassic:t1T8yaLVhNqxA5KJcmiqqFN88e8DNp2PBfF?amount=1.25&label=Fixture"
    private fun field(type: Class<*>, name: String) = type.getDeclaredField(name).apply { isAccessible = true }

    private fun packet(): ByteArray {
        val matrix = QRCodeWriter().encode(uri, BarcodeFormat.QR_CODE, 333, 333)
        return ByteArray(5 + 333 * 333).apply {
            this[0] = 1; this[1] = 77; this[2] = 1; this[3] = 77; this[4] = 1
            for (index in 5 until size) this[index] = if (matrix[(index - 5) % 333, (index - 5) / 333]) 0 else -1
        }
    }

    private inner class Fixture : AutoCloseable {
        val service = ScanDecodeService()
        val worker = field(ScanDecodeService::class.java, "worker").get(service) as OwnedExecutor
        val busy = field(ScanDecodeService::class.java, "busy").get(service) as AtomicBoolean
        val release = CountDownLatch(1)
        val failure = AtomicReference<Throwable?>()
        val endpoint: IScanDecoder
        private val backend: ThreadPoolExecutor?
            get() = field(OwnedExecutor::class.java, "executor").get(worker) as ThreadPoolExecutor?

        init {
            assertEquals("ranchu", Build.HARDWARE)
            assertEquals("org.zclassic.wallet.dev", context.packageName)
            assertEquals(Process.myUid(), context.applicationInfo.uid)
            ContextWrapper::class.java.getDeclaredMethod("attachBaseContext", Context::class.java)
                .apply { isAccessible = true }.invoke(service, context)
            endpoint = IScanDecoder.Stub.asInterface(service.onBind(null))
        }

        fun awaitRelease() {
            if (!release.await(10, TimeUnit.SECONDS)) failure.compareAndSet(null, AssertionError("Public reply gate expired"))
        }

        fun drain() {
            val drained = CountDownLatch(1)
            assertTrue(worker.submit { drained.countDown() })
            assertTrue("Decoder worker did not finish", drained.await(5, TimeUnit.SECONDS))
            failure.get()?.let { throw it }
            assertFalse(busy.get())
        }

        override fun close() {
            release.countDown()
            val pool = backend
            service.onDestroy()
            pool?.let { assertTrue("Decoder worker retained admission", it.awaitTermination(5, TimeUnit.SECONDS)) }
            failure.get()?.let { throw it }
        }
    }

    private fun checkReply(network: Int, throws: Boolean = false): Unit = Fixture().use { fixture ->
        val frame = packet()
        val entered = CountDownLatch(1)
        val retiredAtReply = AtomicBoolean()
        val result = AtomicReference<ByteArray?>()
        val observed = AtomicReference<String?>()
        val calls = AtomicInteger()
        val reply = object : IScanReply.Stub() {
            override fun onReady() = error("No ready request")
            override fun onResult(requestId: Long, text: ByteArray?) {
                try {
                    assertEquals(42L, requestId)
                    assertEquals(1, calls.incrementAndGet())
                    retiredAtReply.set(frame.all { it == 0.toByte() })
                    result.set(text)
                    observed.set(text?.toString(Charsets.UTF_8))
                } catch (problem: Throwable) { fixture.failure.set(problem) }
                finally { entered.countDown() }
                fixture.awaitRelease()
                if (throws) throw IllegalStateException("Public reply marshalling refusal")
            }
        }
        try {
            fixture.endpoint.decode(frame, network, 42L, reply)
            assertTrue("Decoder did not reach reply", entered.await(5, TimeUnit.SECONDS))
            fixture.failure.get()?.let { throw it }
            assertTrue("Frame pixels remained live during reply handoff", retiredAtReply.get())
            assertTrue(fixture.busy.get())
            if (network == 0) {
                assertEquals(uri, observed.get())
                assertTrue(checkNotNull(result.get()).any { it != 0.toByte() })
            } else assertNull(result.get())

            // The held reply must still occupy the service's one input slot.
            val refused = packet()
            var rejections = 0
            try {
                fixture.endpoint.decode(refused, 0, 43L, object : IScanReply.Stub() {
                    override fun onReady() = error("No ready request")
                    override fun onResult(requestId: Long, text: ByteArray?) {
                        assertEquals(43L, requestId)
                        assertNull(text)
                        assertTrue(refused.all { it == 0.toByte() })
                        ++rejections
                    }
                })
                assertEquals(1, rejections)
                assertTrue(fixture.busy.get())
            } finally { refused.fill(0) }

            fixture.release.countDown()
            fixture.drain()
            assertTrue(frame.all { it == 0.toByte() })
            result.get()?.let { assertTrue("Reply text remained live after handoff", it.all { byte -> byte == 0.toByte() }) }
        } finally { fixture.release.countDown(); frame.fill(0) }
    }

    @Test fun decodedPixelsRetireBeforeReplyAndTextAfterReply() = checkReply(network = 0)

    @Test fun wrongNetworkStillRetiresPixelsBeforeRefusal() = checkReply(network = 1)

    @Test fun replyExceptionStillRetiresArraysAndReleasesBusySlot() = checkReply(network = 0, throws = true)

    @Test fun closeRetiresQueuedPixelsWithoutDelivering() = Fixture().use { fixture ->
        val entered = CountDownLatch(1)
        val deliveries = AtomicInteger()
        val frame = packet()
        try {
            assertTrue(fixture.worker.submit { entered.countDown(); fixture.awaitRelease() })
            assertTrue(entered.await(5, TimeUnit.SECONDS))
            fixture.endpoint.decode(frame, 0, 42L, object : IScanReply.Stub() {
                override fun onReady() = error("No ready request")
                override fun onResult(requestId: Long, text: ByteArray?) { deliveries.incrementAndGet() }
            })
            assertTrue(fixture.busy.get())
            assertTrue(frame.any { it != 0.toByte() })
            fixture.service.onDestroy()
            assertTrue(frame.all { it == 0.toByte() })
            assertFalse(fixture.busy.get())
            assertEquals(0, deliveries.get())
            fixture.release.countDown()
        } finally { fixture.release.countDown(); frame.fill(0) }
    }
}
