// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import androidx.test.ext.junit.runners.AndroidJUnit4
import java.nio.ByteBuffer
import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith
import org.zclassic.wallet.core.CameraFrames

/** Public coordinate markers through the packaged JNI. No camera or wallet.
 * Expected sampling steps are explicit; QR error correction cannot hide a
 * misplaced pixel. Every view borrows one fixture-owned direct allocation. */
@RunWith(AndroidJUnit4::class)
class CameraPlaneMappingInstrumentedTest {
    private data class Shape(val width: Int, val height: Int, val step: Int,
                             val outputWidth: Int, val outputHeight: Int)

    private fun marker(x: Int, y: Int) = ((17 * x + 31 * y + 7) and 255).toByte()

    private fun expected(shape: Shape) = ByteArray(5 + shape.outputWidth * shape.outputHeight).apply {
        this[0] = 1
        this[1] = shape.outputWidth.toByte()
        this[2] = (shape.outputWidth ushr 8).toByte()
        this[3] = shape.outputHeight.toByte()
        this[4] = (shape.outputHeight ushr 8).toByte()
        for (i in 5 until size) {
            val pixel = i - 5
            this[i] = marker((pixel % shape.outputWidth) * shape.step,
                (pixel / shape.outputWidth) * shape.step)
        }
    }

    private fun checkView(plane: ByteBuffer, shape: Shape, row: Int, stride: Int, wanted: ByteArray) {
        val position = plane.position()
        val limit = plane.limit()
        val packet = checkNotNull(CameraFrames.pack(plane, shape.width, shape.height, row, stride))
        try {
            assertArrayEquals("Sampled coordinates differ", wanted, packet)
            assertEquals(position, plane.position())
            assertEquals(limit, plane.limit())
        } finally { packet.fill(0) }
    }

    private fun checkUnchanged(plane: ByteBuffer, before: ByteArray) {
        val after = ByteArray(before.size)
        try {
            plane.clear()
            plane.get(after)
            assertArrayEquals("Borrowed plane changed", before, after)
        } finally { after.fill(0) }
    }

    private fun checkPlane(shape: Shape, stride: Int, padded: Boolean) {
        val offset = 13
        val row = shape.width * stride + 7
        val span = (shape.height - 1) * row + (shape.width - 1) * stride + 1
        val length = if (padded) shape.height * row else span
        val plane = ByteBuffer.allocateDirect(offset + length + 19)
        val before = ByteArray(plane.capacity()) { 93 }
        val wanted = expected(shape)
        try {
            for (y in 0 until shape.height) for (x in 0 until shape.width) {
                before[offset + y * row + x * stride] = marker(x, y)
            }
            plane.put(before)
            plane.limit(offset + length)
            plane.position(offset)
            for (view in listOf(plane, plane.asReadOnlyBuffer(), plane.slice())) {
                checkView(view, shape, row, stride, wanted)
            }
            plane.limit(offset + span - 1)
            assertNull("A missing last source pixel must refuse",
                CameraFrames.pack(plane, shape.width, shape.height, row, stride))
            checkUnchanged(plane, before)
        } finally {
            wanted.fill(0)
            before.fill(0)
            plane.clear()
            while (plane.hasRemaining()) plane.put(0)
        }
    }

    @Test fun nonuniformStridedPlanesPreserveEverySampleAndBorrowedView() {
        val shapes = listOf(
            Shape(43, 29, 1, 43, 29),
            Shape(385, 257, 2, 193, 129),
            Shape(1024, 127, 3, 342, 43)
        )
        for (shape in shapes) for (stride in 1..4) for (padded in listOf(false, true)) {
            checkPlane(shape, stride, padded)
        }
    }
}
