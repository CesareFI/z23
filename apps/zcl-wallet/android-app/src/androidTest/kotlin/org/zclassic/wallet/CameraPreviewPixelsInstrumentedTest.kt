// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import java.util.concurrent.atomic.AtomicReference
import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith

/** Public coordinate markers on an unattached software Canvas. This observes
 * natural-display preview pixels, not a camera driver or physical rotation. */
@RunWith(AndroidJUnit4::class)
class CameraPreviewPixelsInstrumentedTest {
    private val instrumentation = InstrumentationRegistry.getInstrumentation()

    private fun onMain(action: () -> Unit) {
        val failure = AtomicReference<Throwable?>()
        instrumentation.runOnMainSync {
            try { action() } catch (problem: Throwable) { failure.set(problem) }
        }
        failure.get()?.let { throw it }
    }

    private fun marker(x: Int, y: Int) = (17 * x + 31 * y + 7) and 255

    private fun packet(width: Int, height: Int) = ByteArray(5 + width * height).apply {
        this[0] = 1
        this[1] = width.toByte()
        this[2] = (width ushr 8).toByte()
        this[3] = height.toByte()
        this[4] = (height ushr 8).toByte()
        for (y in 0 until height) for (x in 0 until width) {
            this[5 + y * width + x] = marker(x, y).toByte()
        }
    }

    private fun rotated(width: Int, height: Int, turns: Int, front: Boolean): IntArray {
        val sideways = turns % 2 != 0
        val outputWidth = if (sideways) height else width
        val output = IntArray(width * height)
        for (y in 0 until height) for (x in 0 until width) {
            val destination = when (turns) {
                0 -> x to y
                1 -> height - 1 - y to x
                2 -> width - 1 - x to height - 1 - y
                else -> y to width - 1 - x
            }
            val dx = if (front) outputWidth - 1 - destination.first else destination.first
            val gray = marker(x, y)
            output[destination.second * outputWidth + dx] = Color.rgb(gray, gray, gray)
        }
        return output
    }

    private fun checkPixels(target: Bitmap, expected: IntArray, frameWidth: Int, frameHeight: Int) {
        // Unit scale avoids mistaking Android's permitted bitmap filtering
        // during magnification for a coordinate-mapping failure.
        val left = (target.width - frameWidth) / 2
        val top = (target.height - frameHeight) / 2
        for (y in 0 until target.height) for (x in 0 until target.width) {
            val inside = x in left until left + frameWidth &&
                y in top until top + frameHeight
            val wanted = if (inside) expected[(y - top) * frameWidth + x - left]
                         else Color.BLACK
            assertEquals("Preview pixel ($x,$y) differs", wanted, target.getPixel(x, y))
        }
    }

    private fun render(width: Int, height: Int, orientation: Int, front: Boolean, padding: Int) {
        val turns = if (front) (4 - orientation / 90) % 4 else orientation / 90
        val frameWidth = if (turns % 2 == 0) width else height
        val frameHeight = if (turns % 2 == 0) height else width
        val targetWidth = frameWidth + if (padding == 1) 16 else 0
        val targetHeight = frameHeight + if (padding == 2) 20 else 0
        val view = CameraPreviewView(instrumentation.targetContext)
        val input = packet(width, height)
        val before = input.copyOf()
        val expected = rotated(width, height, turns, front)
        val target = Bitmap.createBitmap(targetWidth, targetHeight, Bitmap.Config.ARGB_8888)
        val canvas = Canvas(target)
        try {
            assertNull("Fixture only qualifies the natural-display path", view.display)
            view.layout(0, 0, targetWidth, targetHeight)
            target.eraseColor(Color.MAGENTA)
            view.show(input, orientation, front)
            view.draw(canvas)
            checkPixels(target, expected, frameWidth, frameHeight)
            assertArrayEquals("Preview modified the borrowed packet", before, input)
        } finally {
            try { view.clear() }
            finally {
                canvas.setBitmap(null)
                target.eraseColor(Color.BLACK)
                target.recycle()
                input.fill(0)
                before.fill(0)
                expected.fill(0)
            }
        }
    }

    @Test fun everyPixelSurvivesRotationMirroringAndLetterboxing() = onMain {
        for ((width, height) in listOf(43 to 29, 21 to 384)) {
            for (orientation in listOf(0, 90, 180, 270)) {
                for (front in listOf(false, true)) for (padding in 0..2) {
                    render(width, height, orientation, front, padding)
                }
            }
        }
    }
}
