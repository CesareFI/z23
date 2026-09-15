// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet.core

import com.google.zxing.BarcodeFormat
import com.google.zxing.qrcode.QRCodeWriter
import java.nio.ByteBuffer
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import kotlin.test.assertNotNull
import kotlin.test.assertNull

class CameraFramesTest {
    private val address = "t1T8yaLVhNqxA5KJcmiqqFN88e8DNp2PBfF"

    @Test fun directPlaneOffsetLimitAndReadOnlyBuffer() {
        val text = "zclassic:$address?amount=1.25&label=Public%20fixture"
        val matrix = QRCodeWriter().encode(text, BarcodeFormat.QR_CODE, 333, 333)
        val bytes = ByteArray(333 * 333) { if (matrix[it % 333, it / 333]) 0 else -1 }
        val direct = ByteBuffer.allocateDirect(bytes.size + 29)
        direct.position(13)
        direct.put(bytes)
        direct.limit(13 + bytes.size)
        direct.position(13)
        for (plane in listOf(direct, direct.asReadOnlyBuffer(), direct.slice())) {
            val before = plane.position()
            val packet = assertNotNull(CameraFrames.pack(plane, 333, 333, 333, 1))
            assertEquals(before, plane.position())
            val decoded = assertNotNull(CameraFrames.decode(packet, Network.MAINNET))
            assertContentEquals(text.toByteArray(), decoded)
            assertEquals(Zatoshi.of(125000000), PaymentRequest.parseEncoded(decoded, Network.MAINNET)?.amount)
            assertNull(CameraFrames.decode(packet, Network.TESTNET))
            assertNull(PaymentRequest.parseEncoded(decoded, Network.TESTNET))
        }
        val after = ByteArray(bytes.size)
        direct.get(after)
        assertContentEquals(bytes, after)
    }

    @Test fun invalidDirectRangesAndHeapBuffersRefuse() {
        val plane = ByteBuffer.allocateDirect(441)
        assertNull(CameraFrames.pack(ByteBuffer.allocate(441), 21, 21, 21, 1))
        for ((offset, length) in listOf(-1 to 441, 1 to 441, 0 to -1, Int.MAX_VALUE to 441, 0 to 440)) {
            assertRefused(plane, offset, length)
        }
        assertRefused(plane, 0, 441, width = -1)
        assertRefused(plane, 0, 441, row = Int.MAX_VALUE)
        assertRefused(plane, 0, 441, pixel = -1)
        plane.limit(440)
        assertNull(CameraFrames.pack(plane, 21, 21, 21, 1))
    }

    private fun assertRefused(plane: ByteBuffer, offset: Int, length: Int,
                              width: Int = 21, row: Int = 21, pixel: Int = 1) {
        val output = ByteArray(446) { 93 }
        try {
            assertEquals(0, NativeCore.cameraPlanePacketSize(plane, offset, length, width, 21, row, pixel))
            assertEquals(0, NativeCore.packCameraPlane(plane, offset, length, width, 21, row, pixel, output))
            assertContentEquals(ByteArray(446) { 93 }, output)
        } finally { output.fill(0) }
    }

    @Test fun outputCapacityMustMatchTheRevalidatedPlane() {
        val plane = ByteBuffer.allocateDirect(441)
        assertEquals(446, NativeCore.cameraPlanePacketSize(plane, 0, 441, 21, 21, 21, 1))
        for (capacity in listOf(0, 1, 445, 447, CameraFrames.MAX_PACKET_BYTES + 1)) {
            val output = ByteArray(capacity) { 93 }
            try {
                assertEquals(0, NativeCore.packCameraPlane(plane, 0, 441, 21, 21, 21, 1, output))
                assertContentEquals(ByteArray(capacity) { 93 }, output)
            } finally { output.fill(0) }
        }
    }

    @Test fun packetBoundsAndUntrustedTextRefuse() {
        for (packet in listOf(ByteArray(0), ByteArray(4), ByteArray(CameraFrames.MAX_PACKET_BYTES + 1))) {
            assertNull(CameraFrames.decode(packet, Network.MAINNET))
        }
        assertNull(PaymentRequest.parseEncoded(byteArrayOf(-1, -2), Network.MAINNET))
        assertNull(PaymentRequest.parseEncoded("zclassic:$address?amount=1&amount=2".toByteArray(), Network.MAINNET))
    }
}
