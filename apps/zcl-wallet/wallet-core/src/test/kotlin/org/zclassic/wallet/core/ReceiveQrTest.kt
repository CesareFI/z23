// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet.core

import com.google.zxing.BarcodeFormat
import com.google.zxing.BinaryBitmap
import com.google.zxing.DecodeHintType
import com.google.zxing.RGBLuminanceSource
import com.google.zxing.common.HybridBinarizer
import com.google.zxing.common.BitMatrix
import com.google.zxing.qrcode.decoder.Decoder
import com.google.zxing.qrcode.QRCodeReader
import com.google.zxing.multi.qrcode.QRCodeMultiReader
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertNull

class ReceiveQrTest {
    private fun decodeModules(qr: ReceiveQr): String {
        val symbol = BitMatrix(qr.side - 8)
        for (y in 0 until symbol.height) for (x in 0 until symbol.width)
            if (qr.isDark(x + 4, y + 4)) symbol.set(x, y)
        return Decoder().decode(symbol).text
    }

    @Test fun independentModuleDecoderChecksEveryPublicFixture() {
        for (network in Network.entries) repeat(24) { number ->
            val hash = ByteArray(20) { index -> (number * 11 + index * 7).toByte() }
            val address = TransparentAddress.fromPublicKeyHash(hash, network)
            assertEquals(address.encoded, decodeModules(ReceiveQr.forAddress(address)))
        }
    }
    private fun image(qr: ReceiveQr, scale: Int, rotate: Boolean): BinaryBitmap {
        val side = qr.side * scale
        val pixels = IntArray(side * side) { index ->
            val x = index % side / scale
            val y = index / side / scale
            val dark = if (rotate) qr.isDark(y, qr.side - x - 1) else qr.isDark(x, y)
            if (dark) 0xff000000.toInt() else 0xffffffff.toInt()
        }
        return BinaryBitmap(HybridBinarizer(RGBLuminanceSource(side, side, pixels)))
    }

    private fun decode(qr: ReceiveQr, scale: Int, rotate: Boolean): String {
        // Consider every finder-pattern candidate: QR payloads can themselves
        // contain a finder-like pattern. Require one successfully decoded QR.
        val results = QRCodeMultiReader().decodeMultiple(image(qr, scale, rotate),
            mapOf(DecodeHintType.TRY_HARDER to true))
        assertEquals(1, results.size)
        val result = results.single()
        assertEquals(BarcodeFormat.QR_CODE, result.barcodeFormat)
        return result.text
    }

    @Test fun pinnedSingleCandidateDetectorRejectsAValidPublicSymbol() {
        val hash = ByteArray(20) { index -> (16 * 11 + index * 7).toByte() }
        val address = TransparentAddress.fromPublicKeyHash(hash, Network.MAINNET)
        val qr = ReceiveQr.forAddress(address)
        assertEquals(address.encoded, decodeModules(qr))
        assertFailsWith<com.google.zxing.ChecksumException> {
            QRCodeReader().decode(image(qr, 5, true), mapOf(DecodeHintType.TRY_HARDER to true))
        }
        assertEquals(address.encoded, decode(qr, 5, true))
    }

    @Test fun independentDecoderRecoversExactAddressesAtMultipleScalesAndRotations() {
        for (network in Network.entries) {
            repeat(24) { number ->
                val hash = ByteArray(20) { index -> (number * 11 + index * 7).toByte() }
                val address = TransparentAddress.fromPublicKeyHash(hash, network)
                val qr = ReceiveQr.forAddress(address)
                assertEquals(41, qr.side)
                for (scale in listOf(2, 5, 9)) {
                    for (rotated in listOf(false, true)) {
                        val decoded = try { decode(qr, scale, rotated) }
                        catch (error: com.google.zxing.ReaderException) {
                            throw AssertionError("Public QR fixture: network=$network number=$number scale=$scale rotated=$rotated", error)
                        }
                        assertEquals(address.encoded, decoded)
                    }
                }
            }
        }
    }

    @Test fun scriptAddressAndCoordinateBounds() {
        val address = TransparentAddress.parse("t3VDyGHn9mbyCf448m2cHTu5uXvsJpKHbiZ", Network.MAINNET)
        val qr = ReceiveQr.forAddress(address)
        assertEquals(address.encoded, decode(qr, 4, false))
        assertFailsWith<IllegalArgumentException> { qr.isDark(-1, 0) }
        assertFailsWith<IllegalArgumentException> { qr.isDark(0, qr.side) }
        assertFailsWith<IllegalArgumentException> { qr.isDark(Int.MAX_VALUE, Int.MIN_VALUE) }
    }

    @Test fun jniRefusesInvalidAddressNetworkAndOversizedData() {
        val address = "t1T8yaLVhNqxA5KJcmiqqFN88e8DNp2PBfF".toByteArray()
        assertNull(NativeCore.receiveQr(address, Network.TESTNET.nativeId))
        assertNull(NativeCore.receiveQr(address, -1))
        assertNull(NativeCore.receiveQr(ByteArray(36), Network.MAINNET.nativeId))
        assertNull(NativeCore.receiveQr(ByteArray(0), Network.MAINNET.nativeId))
    }
}
