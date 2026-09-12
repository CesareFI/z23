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
            assertNull(NativeCore.packCameraPlane(plane, offset, length, 21, 21, 21, 1))
        }
        assertNull(NativeCore.packCameraPlane(plane, 0, 441, -1, 21, 21, 1))
        assertNull(NativeCore.packCameraPlane(plane, 0, 441, 21, 21, Int.MAX_VALUE, 1))
        assertNull(NativeCore.packCameraPlane(plane, 0, 441, 21, 21, 21, -1))
        plane.limit(440)
        assertNull(CameraFrames.pack(plane, 21, 21, 21, 1))
    }

    @Test fun packetBoundsAndUntrustedTextRefuse() {
        for (packet in listOf(ByteArray(0), ByteArray(4), ByteArray(CameraFrames.MAX_PACKET_BYTES + 1))) {
            assertNull(CameraFrames.decode(packet, Network.MAINNET))
        }
        assertNull(PaymentRequest.parseEncoded(byteArrayOf(-1, -2), Network.MAINNET))
        assertNull(PaymentRequest.parseEncoded("zclassic:$address?amount=1&amount=2".toByteArray(), Network.MAINNET))
    }
}
