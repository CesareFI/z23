// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.graphics.Bitmap
import android.graphics.Canvas
import android.view.View
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import com.google.zxing.BinaryBitmap
import com.google.zxing.DecodeHintType
import com.google.zxing.NotFoundException
import com.google.zxing.RGBLuminanceSource
import com.google.zxing.common.HybridBinarizer
import com.google.zxing.multi.qrcode.QRCodeMultiReader
import org.junit.Assert.assertEquals
import org.junit.Assert.assertThrows
import org.junit.Test
import org.junit.runner.RunWith
import org.zclassic.wallet.core.Network
import org.zclassic.wallet.core.ReceiveQr
import org.zclassic.wallet.core.TransparentAddress

/** Public fixtures only. Pixel buffers stay in this test process and are
 * recycled after decoding. No wallet, screenshot file, secret or camera use.
 */
@RunWith(AndroidJUnit4::class)
class ReceiveQrInstrumentedTest {
    private val instrumentation = InstrumentationRegistry.getInstrumentation()

    private fun render(address: TransparentAddress, width: Int, height: Int): BinaryBitmap {
        require(width in 1..640 && height in 1..640)
        val pixels = IntArray(width * height)
        instrumentation.runOnMainSync {
            val bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888)
            try {
                val view = ReceiveQrView(instrumentation.targetContext)
                view.show(ReceiveQr.forAddress(address))
                view.measure(View.MeasureSpec.makeMeasureSpec(width, View.MeasureSpec.EXACTLY),
                    View.MeasureSpec.makeMeasureSpec(height, View.MeasureSpec.EXACTLY))
                view.layout(0, 0, width, height)
                view.draw(Canvas(bitmap))
                bitmap.getPixels(pixels, 0, width, 0, 0, width, height)
            } finally { bitmap.recycle() }
        }
        return BinaryBitmap(HybridBinarizer(RGBLuminanceSource(width, height, pixels)))
    }

    @Test fun independentDecoderReadsActualAndroidCanvasAtDifferentAspectRatios() {
        for (network in Network.entries) {
            val address = TransparentAddress.fromPublicKeyHash(ByteArray(20), network)
            for ((width, height) in listOf(205 to 205, 513 to 617, 600 to 287)) {
                val results = QRCodeMultiReader().decodeMultiple(render(address, width, height),
                    mapOf(DecodeHintType.TRY_HARDER to true))
                assertEquals(1, results.size)
                assertEquals(address.encoded, results.single().text)
            }
        }
    }

    @Test fun UnreadablySmallViewDoesNotDrawACroppedCode() {
        val address = TransparentAddress.fromPublicKeyHash(ByteArray(20), Network.TESTNET)
        assertThrows(NotFoundException::class.java) {
            QRCodeMultiReader().decodeMultiple(render(address, 20, 20))
        }
    }
}
