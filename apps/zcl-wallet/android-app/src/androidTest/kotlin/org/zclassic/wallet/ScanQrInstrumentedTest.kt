// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import androidx.test.ext.junit.runners.AndroidJUnit4
import com.google.zxing.BarcodeFormat
import com.google.zxing.qrcode.QRCodeWriter
import org.junit.Assert.assertEquals
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Test
import org.junit.runner.RunWith
import org.zclassic.wallet.core.Network
import org.zclassic.wallet.core.ScanQr
import org.zclassic.wallet.core.Zatoshi

@RunWith(AndroidJUnit4::class)
class ScanQrInstrumentedTest {
    @Test fun paddedInterleavedImageKeepsExactBoundsAndPixels() {
        val address = "t1T8yaLVhNqxA5KJcmiqqFN88e8DNp2PBfF"
        val side = 205
        val row = 8192
        val pixel = 4
        val used = (side - 1) * row + (side - 1) * pixel + 1
        val matrix = QRCodeWriter().encode(address, BarcodeFormat.QR_CODE, side, side)
        val pixels = ByteArray(8_388_608) { 42 }
        for (y in 0 until side) for (x in 0 until side)
            pixels[y * row + x * pixel] = if (matrix[x, y]) 0 else -1
        val before = pixels.copyOf()
        try {
            val request = ScanQr.decode(pixels, side, side, row, pixel, Network.MAINNET)
            assertEquals(address, request?.address?.encoded)
            assertArrayEquals(before, pixels)
            assertNull(ScanQr.decode(pixels.copyOf(used - 1), side, side, row, pixel, Network.MAINNET))
            assertNull(ScanQr.decode(pixels.copyOf(pixels.size + 1), side, side, row, pixel, Network.MAINNET))
        } finally {
            pixels.fill(0)
            before.fill(0)
        }
    }

    @Test fun unicodeLayoutSeparatorsCannotEnterARequestThroughQrDecoding() {
        val address = "t1T8yaLVhNqxA5KJcmiqqFN88e8DNp2PBfF"
        var accepted = 0
        for (field in listOf("label", "message")) {
            for (separator in listOf("%E2%80%A8", "%E2%80%A9")) {
                val matrix = QRCodeWriter().encode("zclassic:$address?$field=Public${separator}Amount:%20999",
                    BarcodeFormat.QR_CODE, 333, 333)
                val pixels = ByteArray(333 * 333) { index -> if (matrix[index % 333, index / 333]) 0 else -1 }
                try {
                    if (ScanQr.decode(pixels, 333, 333, 333, 1, Network.MAINNET) != null) ++accepted
                } finally { pixels.fill(0) }
            }
        }
        assertEquals("All four separator/field combinations must be refused", 0, accepted)
    }

    @Test fun publicRequestCrossesRealAndroidJni() {
        val address = "t1T8yaLVhNqxA5KJcmiqqFN88e8DNp2PBfF"
        val matrix = QRCodeWriter().encode("zclassic:$address?amount=1.25&label=Fixture",
            BarcodeFormat.QR_CODE, 333, 333)
        val pixels = ByteArray(333 * 333) { index -> if (matrix[index % 333, index / 333]) 0 else -1 }
        try {
            val request = ScanQr.decode(pixels, 333, 333, 333, 1, Network.MAINNET)
            assertNotNull(request)
            assertEquals(address, request?.address?.encoded)
            assertEquals(Zatoshi.of(125000000), request?.amount)
            assertEquals("Fixture", request?.label)
            assertNull(ScanQr.decode(pixels, 333, 333, 333, 1, Network.TESTNET))
        } finally {
            pixels.fill(0)
        }
    }

    @Test fun malformedLayoutRefusesWithoutWalletOrCamera() {
        val pixels = ByteArray(21 * 21) { -1 }
        try {
            assertNull(ScanQr.decode(pixels, 21, 21, 21, 1, Network.MAINNET))
            assertNull(ScanQr.decode(pixels, 21, 21, Int.MAX_VALUE, 1, Network.MAINNET))
            assertNull(ScanQr.decode(pixels, Int.MAX_VALUE, 21, 21, 1, Network.MAINNET))
        } finally {
            pixels.fill(0)
        }
    }
}
