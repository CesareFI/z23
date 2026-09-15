// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.content.Context
import android.content.ContextWrapper
import android.content.ServiceConnection
import android.graphics.Bitmap
import android.graphics.Color
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.os.Message
import android.os.Process
import android.view.View
import android.view.ViewGroup
import android.widget.LinearLayout
import androidx.test.core.app.ActivityScenario
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import java.lang.reflect.InvocationTargetException
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicInteger
import java.util.concurrent.atomic.AtomicReference
import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith
import org.zclassic.wallet.core.Network

/** An actual isolated-service binding and public preview bitmap. Camera startup
 * is never called: a private Handler injects shutdown-post failures without
 * acquiring camera/worker admission. No wallet, key or storage is opened. */
@RunWith(AndroidJUnit4::class)
class ScanCleanupFailureInstrumentedTest {
    private val instrumentation = InstrumentationRegistry.getInstrumentation()
    private fun field(type: Class<*>, name: String) = type.getDeclaredField(name).apply { isAccessible = true }
    private fun controller(name: String) = field(CameraScanActivity::class.java, name)

    private fun onMain(action: () -> Unit) {
        val failure = AtomicReference<Throwable?>()
        instrumentation.runOnMainSync {
            try { action() } catch (problem: Throwable) { failure.set(problem) }
        }
        failure.get()?.let { throw it }
    }

    private class UnbindingContext(base: Context) : ContextWrapper(base) {
        var problem: Throwable? = null
        var unbinds = 0
        override fun getApplicationContext(): Context = this
        override fun unbindService(connection: ServiceConnection) {
            super.unbindService(connection)
            ++unbinds
            problem?.let { throw it }
        }
    }

    private class ClosingHandler : Handler(Looper.getMainLooper()) {
        var problem: Throwable? = null
        var enqueue = false
        override fun sendMessageAtTime(message: Message, uptimeMillis: Long): Boolean {
            problem?.let {
                if (enqueue) check(super.sendMessageAtTime(message, uptimeMillis))
                throw it
            }
            return super.sendMessageAtTime(message, uptimeMillis)
        }
    }

    private inner class Fixture(val activity: CameraScanActivity) : AutoCloseable {
        val context = UnbindingContext(instrumentation.targetContext)
        val screens = controller("screens").get(activity) as ScanScreens
        private val ready = CountDownLatch(1)
        private val failures = AtomicInteger()
        private val frames = AtomicInteger()
        val decoder = ScanDecodeClient(context, { ready.countDown() }, { failures.incrementAndGet(); ready.countDown() })
        val camera = CameraCapture(context, { _, _, _, _ -> frames.incrementAndGet() }, { failures.incrementAndGet() })
        val handler = ClosingHandler()
        val packet = ByteArray(5 + 21 * 21) { 93 }.apply {
            this[0] = 1; this[1] = 21; this[2] = 0; this[3] = 21; this[4] = 0
        }
        val root = field(ScanScreens::class.java, "root").get(screens) as LinearLayout
        private lateinit var preview: CameraPreviewView
        private lateinit var bitmap: Bitmap
        var screenClearCalls = 0

        fun connect() {
            onMain { decoder.connect() }
            assertTrue("Isolated decoder did not become ready", ready.await(20, TimeUnit.SECONDS))
            assertEquals(0, failures.get())
            assertTrue(decoder.isReady)
            assertNotEquals(Process.myUid(), decoder.decoderUid)
        }

        fun prepare() {
            field(CameraCapture::class.java, "handler").set(camera, handler)
            controller("camera").set(activity, camera)
            controller("decoder").set(activity, decoder)
            screens.scanning(Network.TESTNET) {}
            screens.frame(packet, 0, false)
            preview = field(ScanScreens::class.java, "preview").get(screens) as CameraPreviewView
            bitmap = field(CameraPreviewView::class.java, "bitmap").get(preview) as Bitmap
            assertTrue(preview.hasFrame)
            assertEquals(Color.rgb(93, 93, 93), bitmap.getPixel(0, 0))
            (field(CameraCapture::class.java, "framePending").get(camera) as AtomicBoolean).set(true)
            CameraCapture::class.java.getDeclaredMethod("dispatch", ByteArray::class.java)
                .apply { isAccessible = true }.invoke(camera, packet)
        }

        fun failScreenClear(problem: Throwable) {
            root.setOnHierarchyChangeListener(object : ViewGroup.OnHierarchyChangeListener {
                override fun onChildViewAdded(parent: View?, child: View?) = Unit
                override fun onChildViewRemoved(parent: View?, child: View?) {
                    ++screenClearCalls
                    throw problem
                }
            })
        }

        fun pause() {
            try {
                CameraScanActivity::class.java.getDeclaredMethod("onPause").apply { isAccessible = true }.invoke(activity)
            } catch (wrapped: InvocationTargetException) { throw checkNotNull(wrapped.cause) }
        }

        fun assertCleared(screenFailure: Boolean) {
            assertEquals(false, controller("resumed").get(activity))
            assertTrue((field(CameraCapture::class.java, "closed").get(camera) as AtomicBoolean).get())
            assertTrue("Camera failure skipped decoder retirement",
                (field(ScanDecodeClient::class.java, "closed").get(decoder) as AtomicBoolean).get())
            assertFalse((field(ScanDecodeClient::class.java, "bound").get(decoder) as AtomicBoolean).get())
            assertEquals("Decoder was not actually unbound", 1, context.unbinds)
            assertTrue(packet.all { it == 0.toByte() })
            assertNull((field(CameraCapture::class.java, "queuedPacket").get(camera) as AtomicReference<*>).get())
            assertFalse("Shutdown failure retained preview pixels", preview.hasFrame)
            assertEquals(Color.BLACK, bitmap.getPixel(0, 0))
            if (screenFailure) assertTrue(screenClearCalls > 0) else assertEquals(0, root.childCount)
            if (handler.problem == null) assertNull(controller("camera").get(activity))
            else assertSame("Failed close must remain available for later cleanup", camera, controller("camera").get(activity))
            if (context.problem == null) assertNull(controller("decoder").get(activity))
            else assertSame(decoder, controller("decoder").get(activity))
        }

        fun assertNoDelivery() {
            instrumentation.waitForIdleSync()
            assertEquals(0, frames.get())
            assertEquals(0, failures.get())
        }

        override fun close() {
            onMain {
                handler.problem = null
                context.problem = null
                root.setOnHierarchyChangeListener(null)
                try { camera.close() }
                finally { try { decoder.close() } finally { screens.clear(); packet.fill(0) } }
            }
            instrumentation.waitForIdleSync()
            onMain { handler.removeCallbacksAndMessages(null) }
        }
    }

    private fun checkCleanup(cameraFailure: Throwable? = null, enqueue: Boolean = false,
                             decoderFailure: Throwable? = null, screenFailure: Throwable? = null) {
        assertEquals("ranchu", Build.HARDWARE)
        assertEquals("org.zclassic.wallet.dev", instrumentation.targetContext.packageName)
        ActivityScenario.launch(CameraScanActivity::class.java).use { scenario ->
            val fixture = AtomicReference<Fixture>()
            scenario.onActivity { fixture.set(Fixture(it)) }
            fixture.get().use { owned ->
                owned.connect()
                onMain {
                    owned.prepare()
                    owned.handler.problem = cameraFailure
                    owned.handler.enqueue = enqueue
                    owned.context.problem = decoderFailure
                    screenFailure?.let(owned::failScreenClear)
                    val expected = cameraFailure ?: decoderFailure ?: screenFailure
                    if (expected == null) owned.pause()
                    else assertSame(expected, assertThrows(Throwable::class.java) { owned.pause() })
                    owned.assertCleared(screenFailure != null)
                }
                owned.assertNoDelivery()
            }
        }
    }

    @Test fun cameraCloseExceptionsStillRetireDecoderAndPreview() {
        for (enqueue in listOf(false, true)) checkCleanup(
            cameraFailure = IllegalStateException("Public shutdown-post refusal"), enqueue = enqueue)
    }

    @Test fun cameraCloseErrorsStillRetireDecoderAndPreview() {
        for (enqueue in listOf(false, true)) checkCleanup(
            cameraFailure = OutOfMemoryError("Public shutdown-post failure"), enqueue = enqueue)
    }

    @Test fun decoderUnbindFailureStillClearsPreview() =
        checkCleanup(decoderFailure = OutOfMemoryError("Public failure after actual unbind"))

    @Test fun secondaryCleanupErrorsPreserveTheFirstFailure() = checkCleanup(
        cameraFailure = OutOfMemoryError("Public primary shutdown failure"), enqueue = true,
        decoderFailure = OutOfMemoryError("Public secondary unbind failure"),
        screenFailure = OutOfMemoryError("Public secondary screen-clear failure"))

    @Test fun ordinaryPauseClearsOwnersAndPreview() = checkCleanup()
}
