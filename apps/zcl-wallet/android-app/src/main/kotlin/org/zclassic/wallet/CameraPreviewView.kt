// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.content.Context
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.view.Surface
import android.view.View
import kotlin.math.min
import org.zclassic.wallet.core.CameraFrames

/** Bounded grayscale presentation of the exact C-sampled frame. No detection,
 * request parsing or camera ownership. Called only on the UI thread. */
internal class CameraPreviewView(context: Context) : View(context) {
    private var bitmap: Bitmap? = null
    private var pixels = IntArray(0)
    private var sensorRotation = 0
    private var front = false
    private val paint = Paint()
    internal val hasFrame: Boolean get() = bitmap != null

    init {
        id = R.id.scan_preview
        isSaveEnabled = false
        contentDescription = context.getString(R.string.scan_preview_description)
        setBackgroundColor(Color.BLACK)
    }

    fun show(packet: ByteArray, orientation: Int, facingFront: Boolean) {
        require(packet.size in 5..CameraFrames.MAX_PACKET_BYTES && packet[0] == 1.toByte())
        val width = (packet[1].toInt() and 255) or ((packet[2].toInt() and 255) shl 8)
        val height = (packet[3].toInt() and 255) or ((packet[4].toInt() and 255) shl 8)
        require(width in 21..384 && height in 21..384 && packet.size == 5 + width * height)
        if (bitmap?.width != width || bitmap?.height != height) {
            clear()
            bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888)
            pixels = IntArray(width * height)
        }
        for (index in pixels.indices) {
            val gray = packet[index + 5].toInt() and 255
            pixels[index] = Color.rgb(gray, gray, gray)
        }
        bitmap?.setPixels(pixels, 0, width, 0, 0, width, height)
        pixels.fill(0)
        sensorRotation = orientation
        front = facingFront
        invalidate()
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        val image = bitmap ?: return
        val displayDegrees = when (display?.rotation) {
            Surface.ROTATION_90 -> 90
            Surface.ROTATION_180 -> 180
            Surface.ROTATION_270 -> 270
            else -> 0
        }
        // Camera2 sensor-to-display rotation, then front-facing preview mirror.
        val rotation = if (front) (displayDegrees - sensorRotation + 360) % 360
                       else (sensorRotation + displayDegrees) % 360
        val sideways = rotation % 180 != 0
        val rotatedWidth = if (sideways) image.height else image.width
        val rotatedHeight = if (sideways) image.width else image.height
        val scale = min(width.toFloat() / rotatedWidth, height.toFloat() / rotatedHeight)
        val saved = canvas.save()
        canvas.translate(width / 2f, height / 2f)
        canvas.scale(if (front) -scale else scale, scale)
        canvas.rotate(rotation.toFloat())
        canvas.drawBitmap(image, -image.width / 2f, -image.height / 2f, paint)
        canvas.restoreToCount(saved)
    }

    fun clear() {
        bitmap?.eraseColor(Color.BLACK)
        bitmap = null // Let Android release any rendering references; do not recycle a queued bitmap.
        pixels.fill(0)
        pixels = IntArray(0)
        invalidate()
    }

    override fun onDetachedFromWindow() {
        clear()
        super.onDetachedFromWindow()
    }
}
