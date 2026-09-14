// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.graphics.Bitmap
import android.graphics.Color
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import java.util.concurrent.atomic.AtomicReference
import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith

/** Public grayscale markers in unattached views only. Real Android bitmap
 * refusals replace a platform failure; no camera, wallet or key is accessed. */
@RunWith(AndroidJUnit4::class)
class CameraPreviewFailureInstrumentedTest {
    private val instrumentation = InstrumentationRegistry.getInstrumentation()

    private fun field(name: String) = CameraPreviewView::class.java.getDeclaredField(name).apply {
        isAccessible = true
    }

    private fun onMain(action: () -> Unit) {
        val failure = AtomicReference<Throwable?>()
        instrumentation.runOnMainSync {
            try { action() } catch (problem: Throwable) { failure.set(problem) }
        }
        failure.get()?.let { throw it }
    }

    private fun packet(side: Int = 21) = ByteArray(5 + side * side) { 93 }.apply {
        this[0] = 1
        this[1] = side.toByte()
        this[2] = (side ushr 8).toByte()
        this[3] = side.toByte()
        this[4] = (side ushr 8).toByte()
    }

    private fun image(view: CameraPreviewView) = field("bitmap").get(view) as Bitmap
    private fun pixels(view: CameraPreviewView) = field("pixels").get(view) as IntArray

    @Test fun failedBitmapEraseStillClearsAndRetiresOwnedBuffers() = onMain {
        val view = CameraPreviewView(instrumentation.targetContext)
        view.show(packet(), 0, false)
        val ownedPixels = pixels(view)
        val rejected = image(view)
        try {
            ownedPixels.fill(0x123456) // Model a partially completed pixel copy.
            rejected.recycle() // Never attached or queued for rendering.
            assertThrows(IllegalStateException::class.java) { view.clear() }
            assertTrue("Failed bitmap erase retained copied pixels", ownedPixels.all { it == 0 })
            assertFalse("Failed bitmap erase retained its image", view.hasFrame)
            assertEquals(0, pixels(view).size)
            view.clear() // Failed resources have already been retired.
        } finally {
            field("bitmap").set(view, null) // Recover the old failing implementation only.
            view.clear()
            ownedPixels.fill(0)
        }
    }

    @Test fun failedPixelUploadClearsItsCopyAndRetiresTheFrame() = onMain {
        val view = CameraPreviewView(instrumentation.targetContext)
        val input = packet()
        val original = input.copyOf()
        view.show(input, 0, false)
        val ownedPixels = pixels(view)
        val mutable = image(view)
        val rejected = checkNotNull(mutable.copy(Bitmap.Config.ARGB_8888, false))
        assertFalse(rejected.isMutable)
        field("bitmap").set(view, rejected)
        try {
            assertThrows(IllegalStateException::class.java) { view.show(input, 0, false) }
            assertTrue("Failed pixel upload retained copied pixels", ownedPixels.all { it == 0 })
            assertFalse("Failed pixel upload retained its frame", view.hasFrame)
            assertEquals(0, pixels(view).size)
            assertArrayEquals("Preview must not modify its borrowed packet", original, input)
            view.clear()
            view.show(input, 0, false)
            assertTrue(view.hasFrame)
            assertNotSame(rejected, image(view))
        } finally {
            if (field("bitmap").get(view) === rejected) field("bitmap").set(view, null)
            view.clear()
            ownedPixels.fill(0)
            mutable.eraseColor(Color.BLACK)
            mutable.recycle()
            rejected.recycle() // Unattached test-only copy.
            input.fill(0)
            original.fill(0)
        }
    }

    @Test fun rejectedPacketRetiresThePreviouslyDisplayedFrame() = onMain {
        val view = CameraPreviewView(instrumentation.targetContext)
        try {
            for (invalid in listOf(byteArrayOf(), packet().apply { this[0] = 2 }, packet().copyOf(17))) {
                view.show(packet(), 0, false)
                val previous = image(view)
                assertThrows(IllegalArgumentException::class.java) { view.show(invalid, 0, false) }
                assertFalse("Rejected packet retained an older frame", view.hasFrame)
                assertEquals(Color.BLACK, previous.getPixel(0, 0))
                assertEquals(0, pixels(view).size)
            }
        } finally { view.clear() }
    }

    @Test fun successfulUploadsClearScratchAndReuseTheEmptyBuffer() = onMain {
        val view = CameraPreviewView(instrumentation.targetContext)
        val empty = pixels(view)
        try {
            for (side in listOf(21, 384, 21)) {
                val input = packet(side)
                try {
                    view.show(input, 90, true)
                    assertEquals(Color.rgb(93, 93, 93), image(view).getPixel(side - 1, side - 1))
                    assertTrue(pixels(view).all { it == 0 })
                    val previous = image(view)
                    view.clear()
                    assertFalse(view.hasFrame)
                    assertEquals(Color.BLACK, previous.getPixel(0, 0))
                    assertSame("Cleanup must not allocate another empty buffer", empty, pixels(view))
                    view.clear()
                    assertSame(empty, pixels(view))
                } finally { input.fill(0) }
            }
        } finally { view.clear() }
    }
}
