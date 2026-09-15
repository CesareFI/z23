// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.Manifest
import android.content.Context
import android.content.pm.PackageManager
import android.graphics.ImageFormat
import android.hardware.camera2.CameraCaptureSession
import android.hardware.camera2.CameraCharacteristics
import android.hardware.camera2.CameraDevice
import android.hardware.camera2.CameraManager
import android.hardware.camera2.CaptureRequest
import android.hardware.camera2.params.OutputConfiguration
import android.hardware.camera2.params.SessionConfiguration
import android.media.ImageReader
import android.os.Handler
import android.os.HandlerThread
import android.os.Looper
import android.os.SystemClock
import android.util.Size
import java.util.concurrent.Executor
import java.util.concurrent.RejectedExecutionException
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicReference
import org.zclassic.wallet.core.CameraFrames

/** Camera2 platform owner. All camera resources and borrowed Image planes are
 * accessed/closed on one handler, including close requests from the Activity.
 * No Image or native pointer crosses that thread. One owned packet may be in
 * flight; older camera frames are acquired and closed without copying. */
internal class CameraCapture(
    context: Context,
    private val onFrame: (CameraCapture, ByteArray, Int, Boolean) -> Unit,
    private val onFailure: (CameraCapture) -> Unit,
    private val newWorker: () -> HandlerThread,
) : AutoCloseable {
    constructor(context: Context, onFrame: (CameraCapture, ByteArray, Int, Boolean) -> Unit,
                onFailure: (CameraCapture) -> Unit) :
        this(context, onFrame, onFailure, { HandlerThread("WalletCamera") })

    private val context = context.applicationContext
    private val main = Handler(Looper.getMainLooper())
    private val closed = AtomicBoolean()
    private val framePending = AtomicBoolean()
    private val released = AtomicBoolean()
    private val ownsCamera = AtomicBoolean()
    private val queuedPacket = AtomicReference<ByteArray?>()
    private var thread: HandlerThread? = null
    private var handler: Handler? = null
    // Below are camera-thread owned, after start() posts its first operation.
    private var opening = false
    private var device: CameraDevice? = null
    private var session: CameraCaptureSession? = null
    private var reader: ImageReader? = null
    private var orientation = 0
    private var front = false
    private var autofocus = false
    private var nextFrame = 0L
    private val startupTimeout = Runnable { fail() }

    companion object {
        // A pending OS open must finish before another capture owner starts.
        // A driver that never replies can retain at most one pending owner.
        private val cameraOwner = AtomicBoolean()
    }

    fun start() {
        if (closed.get() || thread != null) return
        if (!cameraOwner.compareAndSet(false, true)) { fail(); return }
        ownsCamera.set(true)
        try {
            val worker = newWorker()
            thread = worker
            worker.start()
            val queue = Handler(worker.looper)
            handler = queue
            if (!main.postDelayed(startupTimeout, 15_000) || !queue.post { open() }) fail()
        } catch (_: Exception) {
            fail()
        } finally {
            // Until the handler is published no OS camera open can be queued.
            // Return admission even if worker construction/start throws Error;
            // fatal errors still propagate after the failed lifetime is closed.
            if (handler == null) {
                closed.set(true)
                try { thread?.quitSafely() }
                finally { releaseOwnership() }
            }
        }
    }

    private fun choose(manager: CameraManager): Pair<String, CameraCharacteristics>? {
        val candidates = manager.cameraIdList.map { it to manager.getCameraCharacteristics(it) }
        return candidates.firstOrNull { it.second[CameraCharacteristics.LENS_FACING] == CameraCharacteristics.LENS_FACING_BACK }
            ?: candidates.firstOrNull()
    }

    private fun size(info: CameraCharacteristics): Size? =
        info[CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP]
            ?.getOutputSizes(ImageFormat.YUV_420_888)
            ?.filter { it.width in 240..1024 && it.height in 240..1024 }
            ?.filter { it.width.toLong() * it.height <= 640L * 480 }
            ?.maxByOrNull { it.width.toLong() * it.height }

    private fun open() {
        if (closed.get()) { release(); return }
        try {
            if (context.checkSelfPermission(Manifest.permission.CAMERA) != PackageManager.PERMISSION_GRANTED) {
                fail(); return
            }
            val manager = context.getSystemService(CameraManager::class.java)
            val selected = choose(manager) ?: run { fail(); return }
            val dimensions = size(selected.second) ?: run { fail(); return }
            orientation = selected.second[CameraCharacteristics.SENSOR_ORIENTATION] ?: 0
            front = selected.second[CameraCharacteristics.LENS_FACING] == CameraCharacteristics.LENS_FACING_FRONT
            autofocus = selected.second[CameraCharacteristics.CONTROL_AF_AVAILABLE_MODES]
                ?.contains(CaptureRequest.CONTROL_AF_MODE_CONTINUOUS_PICTURE) == true
            val images = ImageReader.newInstance(dimensions.width, dimensions.height, ImageFormat.YUV_420_888, 2)
            reader = images
            images.setOnImageAvailableListener(::imageAvailable, handler)
            opening = true
            try { manager.openCamera(selected.first, callbacks, handler) }
            catch (problem: Exception) { opening = false; throw problem }
        } catch (_: Exception) { fail() }
    }

    private val callbacks = object : CameraDevice.StateCallback() {
        override fun onOpened(camera: CameraDevice) {
            opening = false
            if (closed.get()) { camera.close(); release(); return }
            device = camera
            configure(camera)
        }
        override fun onDisconnected(camera: CameraDevice) = lost(camera)
        override fun onError(camera: CameraDevice, error: Int) = lost(camera)
    }

    private fun lost(camera: CameraDevice) {
        opening = false
        camera.close()
        if (device === camera) device = null
        fail()
        if (closed.get()) release()
    }

    private fun configure(camera: CameraDevice) {
        try {
            val output = reader?.surface ?: run { fail(); return }
            val queue = requireNotNull(handler)
            val executor = Executor { task ->
                if (!queue.post(task)) throw RejectedExecutionException("Camera owner closed")
            }
            camera.createCaptureSession(SessionConfiguration(SessionConfiguration.SESSION_REGULAR,
                listOf(OutputConfiguration(output)), executor, object : CameraCaptureSession.StateCallback() {
                    override fun onConfigured(configured: CameraCaptureSession) {
                        if (closed.get()) { configured.close(); return }
                        session = configured
                        repeat(camera, configured)
                    }
                    override fun onConfigureFailed(configured: CameraCaptureSession) {
                        configured.close()
                        fail()
                    }
                }))
        } catch (_: Exception) { fail() }
    }

    private fun repeat(camera: CameraDevice, configured: CameraCaptureSession) {
        try {
            val output = reader?.surface ?: run { fail(); return }
            val request = camera.createCaptureRequest(CameraDevice.TEMPLATE_PREVIEW)
            request.addTarget(output)
            if (autofocus) request.set(CaptureRequest.CONTROL_AF_MODE, CaptureRequest.CONTROL_AF_MODE_CONTINUOUS_PICTURE)
            if (configured.setRepeatingRequest(request.build(), null, handler) < 0) fail()
        } catch (_: Exception) { fail() }
    }

    private fun imageAvailable(images: ImageReader) {
        try {
            val image = images.acquireLatestImage() ?: return
            try {
                if (closed.get()) return
                main.removeCallbacks(startupTimeout)
                val now = SystemClock.elapsedRealtime()
                if (now < nextFrame || !framePending.compareAndSet(false, true)) return
                nextFrame = now + 250
                val plane = image.planes.firstOrNull() ?: run { fail(); return }
                val packet = CameraFrames.pack(plane.buffer, image.width, image.height, plane.rowStride, plane.pixelStride)
                    ?: run { fail(); return }
                dispatch(packet)
            } finally {
                image.close()
            }
        } catch (_: Exception) { fail() }
    }

    private fun dispatch(packet: ByteArray) {
        queuedPacket.set(packet)
        var posted = false
        try {
            if (closed.get()) return
            posted = main.post {
                val owned = queuedPacket.getAndSet(null) ?: return@post
                if (closed.get()) { owned.fill(0); frameDone() }
                else {
                    try { onFrame(this, owned, orientation, front) }
                    finally { owned.fill(0) }
                }
            }
        } finally {
            // Callback allocation or Handler.post may throw. Retire any still
            // queued owner so a late callback cannot claim its pixels.
            if (!posted) {
                queuedPacket.getAndSet(null)?.fill(0)
                frameDone()
            }
        }
        if (!posted) fail()
    }

    fun frameDone() { framePending.set(false) }

    private fun fail() {
        if (closed.compareAndSet(false, true)) {
            close()
            if (!main.post { onFailure(this) }) close()
        }
    }

    override fun close() {
        closed.set(true)
        main.removeCallbacks(startupTimeout)
        queuedPacket.getAndSet(null)?.fill(0)
        // If posting refuses, this owner's looper has already completed release.
        handler?.let { if (!it.post { release() }) frameDone() }
    }

    private fun release() {
        // Keep the handler alive for a pending open's terminal callback.
        if (opening || !released.compareAndSet(false, true)) return
        try { session?.close() }
        finally {
            session = null
            try { device?.close() }
            finally {
                device = null
                try { reader?.close() }
                finally {
                    reader = null
                    frameDone()
                    releaseOwnership()
                    thread?.quitSafely()
                }
            }
        }
    }

    private fun releaseOwnership() {
        if (ownsCamera.compareAndSet(true, false)) cameraOwner.set(false)
    }
}
