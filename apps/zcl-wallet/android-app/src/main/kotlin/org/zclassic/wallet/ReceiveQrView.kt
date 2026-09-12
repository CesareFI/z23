// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.view.View
import org.zclassic.wallet.core.ReceiveQr

/** Platform drawing only. C supplies validated public modules and quiet zone.
 * Integer pixel scaling keeps edges sharp; no bitmap or external QR service.
 */
internal class ReceiveQrView(context: Context) : View(context) {
    private val ink = Paint().apply { color = Color.BLACK; isAntiAlias = false }
    private var qr: ReceiveQr? = null

    fun show(value: ReceiveQr) { qr = value; invalidate() }

    init {
        isSaveEnabled = false
        contentDescription = context.getString(R.string.receiving_qr_description)
    }

    override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
        val desired = (280 * resources.displayMetrics.density).toInt()
        setMeasuredDimension(resolveSize(desired, widthMeasureSpec), resolveSize(desired, heightMeasureSpec))
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        canvas.drawColor(Color.WHITE)
        val qr = qr ?: return
        val scale = minOf(width, height) / qr.side
        if (scale < 1) return
        val left = (width - qr.side * scale) / 2
        val top = (height - qr.side * scale) / 2
        for (y in 0 until qr.side) {
            for (x in 0 until qr.side) {
                if (qr.isDark(x, y)) canvas.drawRect((left + x * scale).toFloat(),
                    (top + y * scale).toFloat(), (left + (x + 1) * scale).toFloat(),
                    (top + (y + 1) * scale).toFloat(), ink)
            }
        }
    }
}
