// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet.core

import com.google.zxing.BarcodeFormat
import com.google.zxing.EncodeHintType
import com.google.zxing.qrcode.QRCodeWriter
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import kotlin.test.assertNotNull
import kotlin.test.assertNull

class ScanQrTest {
    private val address = "t1T8yaLVhNqxA5KJcmiqqFN88e8DNp2PBfF"

    private fun pixels(text: String, side: Int = 205): ByteArray {
        val matrix = QRCodeWriter().encode(text, BarcodeFormat.QR_CODE, side, side,
            mapOf(EncodeHintType.CHARACTER_SET to "UTF-8", EncodeHintType.MARGIN to 4))
        return ByteArray(side * side) { index -> if (matrix[index % side, index / side]) 0 else -1 }
    }

    @Test fun independentEncoderAndUnchangedInput() {
        val image = pixels(address)
        val before = image.copyOf()
        val result = assertNotNull(ScanQr.decode(image, 205, 205, 205, 1, Network.MAINNET))
        assertEquals(address, result.address.encoded)
        assertNull(result.amount)
        assertContentEquals(before, image)
        assertNull(ScanQr.decode(image, 205, 205, 205, 1, Network.TESTNET))
    }

    @Test fun paymentMetadataIsParsedInC() {
        val image = pixels("zclassic:$address?amount=0.00000001&label=Public%20fixture&message=Hello", 333)
        val result = assertNotNull(ScanQr.decode(image, 333, 333, 333, 1, Network.MAINNET))
        assertEquals(address, result.address.encoded)
        assertEquals(Zatoshi.of(1), result.amount)
        assertEquals("Public fixture", result.label)
        assertEquals("Hello", result.message)
    }

    @Test fun rotatedMirroredAndInterleaved() {
        val plain = pixels(address)
        for (mirror in listOf(false, true)) {
            val image = ByteArray(205 * 827) { 42 }
            for (y in 0 until 205) for (x in 0 until 205) {
                val sourceX = if (mirror) 204 - x else x
                image[x * 827 + (204 - y) * 4] = plain[y * 205 + sourceX]
            }
            val result = assertNotNull(ScanQr.decode(image, 205, 205, 827, 4, Network.MAINNET))
            assertEquals(address, result.address.encoded)
        }
    }

    @Test fun malformedSpansAndBlankFramesRefuse() {
        val image = pixels(address)
        assertNull(NativeCore.scanQr(image, -1, 205, 205, 1, 0))
        assertNull(NativeCore.scanQr(image, 205, -1, 205, 1, 0))
        assertNull(NativeCore.scanQr(image, 205, 205, -1, 1, 0))
        assertNull(NativeCore.scanQr(image, 205, 205, 205, -1, 0))
        assertNull(NativeCore.scanQr(image, 205, 205, 205, 1, 2))
        assertNull(ScanQr.decode(image, Int.MAX_VALUE, 205, 205, 1, Network.MAINNET))
        assertNull(ScanQr.decode(image, 205, 205, Int.MAX_VALUE, 1, Network.MAINNET))
        assertNull(ScanQr.decode(image.copyOf(image.size - 1), 205, 205, 205, 1, Network.MAINNET))
        assertNull(ScanQr.decode(ByteArray(8_388_609), 205, 205, 205, 1, Network.MAINNET))
        assertNull(ScanQr.decode(ByteArray(205 * 205) { -1 }, 205, 205, 205, 1, Network.MAINNET))
    }

    @Test fun unsupportedPayloadsAreNotRequests() {
        for (text in listOf("https://example.invalid/", "zclassic:$address?amount=0",
            "zclassic:$address?amount=1&amount=2", "zclassic:$address?req-unsupported=1")) {
            val image = pixels(text, 333)
            assertNull(ScanQr.decode(image, 333, 333, 333, 1, Network.MAINNET))
        }
    }

    @Test fun multipleCodesRequireUserToIsolateOne() {
        val code = pixels(address)
        val image = ByteArray(450 * 225) { -1 }
        for (y in 0 until 205) for (x in 0 until 205) {
            image[(y + 10) * 450 + x + 10] = code[y * 205 + x]
            image[(y + 10) * 450 + x + 235] = code[y * 205 + x]
        }
        assertNull(ScanQr.decode(image, 450, 225, 450, 1, Network.MAINNET))
    }
}
