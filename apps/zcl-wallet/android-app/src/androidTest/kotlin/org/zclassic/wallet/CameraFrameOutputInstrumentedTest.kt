// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import androidx.test.ext.junit.runners.AndroidJUnit4
import java.nio.ByteBuffer
import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith
import org.zclassic.wallet.core.CameraFrames

/** Public uniform pixels through the packaged JNI. No camera, permission,
 * wallet, key or network. JNI failure injection lives in the host VM fixture. */
@RunWith(AndroidJUnit4::class)
class CameraFrameOutputInstrumentedTest {
    private fun clear(buffer: ByteBuffer) {
        buffer.clear()
        while (buffer.hasRemaining()) buffer.put(0)
    }

    private fun checkPacket(plane: ByteBuffer, width: Int, height: Int, sampledWidth: Int, sampledHeight: Int) {
        val position = plane.position()
        val limit = plane.limit()
        val packet = checkNotNull(CameraFrames.pack(plane, width, height, width, 1))
        try {
            assertEquals(5 + sampledWidth * sampledHeight, packet.size)
            assertEquals(1, packet[0].toInt())
            assertEquals(sampledWidth, (packet[1].toInt() and 255) + (packet[2].toInt() and 255) * 256)
            assertEquals(sampledHeight, (packet[3].toInt() and 255) + (packet[4].toInt() and 255) * 256)
            assertTrue("Sampled pixels differ", (5 until packet.size).all { packet[it] == 93.toByte() })
            assertEquals(position, plane.position())
            assertEquals(limit, plane.limit())
        } finally { packet.fill(0) }
    }

    @Test fun exactSampledPacketsPreserveBorrowedOffsetsAndReadOnlyPlanes() {
        val layouts = listOf(
            intArrayOf(640, 480, 320, 240), intArrayOf(480, 640, 240, 320),
            intArrayOf(320, 240, 320, 240), intArrayOf(240, 240, 240, 240),
            intArrayOf(384, 384, 384, 384), intArrayOf(385, 385, 193, 193),
            intArrayOf(1024, 1024, 342, 342), intArrayOf(21, 21, 21, 21)
        )
        for ((width, height, sampledWidth, sampledHeight) in layouts) {
            val plane = ByteBuffer.allocateDirect(width * height + 29)
            try {
                while (plane.hasRemaining()) plane.put(93)
                plane.limit(13 + width * height)
                plane.position(13)
                for (view in listOf(plane, plane.asReadOnlyBuffer(), plane.slice())) {
                    checkPacket(view, width, height, sampledWidth, sampledHeight)
                }
                plane.clear()
                assertTrue("Borrowed plane changed", (0 until plane.capacity()).all { plane.get(it) == 93.toByte() })
            } finally { clear(plane) }
        }
    }

    @Test fun unusablePlanesRefuseAndLeaveTheVmUsable() {
        val pixels = ByteArray(441) { 93 }
        val plane = ByteBuffer.allocateDirect(pixels.size)
        try {
            plane.put(pixels)
            plane.flip()
            assertNull(CameraFrames.pack(ByteBuffer.wrap(pixels), 21, 21, 21, 1))
            assertNull(CameraFrames.pack(plane, -1, 21, 21, 1))
            assertNull(CameraFrames.pack(plane, 21, Int.MAX_VALUE, 21, 1))
            assertNull(CameraFrames.pack(plane, 21, 21, Int.MAX_VALUE, 1))
            assertNull(CameraFrames.pack(plane, 21, 21, 21, -1))
            plane.limit(440)
            assertNull(CameraFrames.pack(plane, 21, 21, 21, 1))
            plane.limit(441)
            plane.position(1)
            assertNull(CameraFrames.pack(plane, 21, 21, 21, 1))
            plane.position(0)
            checkPacket(plane, 21, 21, 21, 21)
            for (i in pixels.indices) assertEquals(pixels[i], plane.get(i))
        } finally { pixels.fill(0); clear(plane) }
    }
}
