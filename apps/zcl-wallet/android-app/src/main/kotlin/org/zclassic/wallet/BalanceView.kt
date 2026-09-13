// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.content.Context
import android.graphics.Color
import android.os.Parcelable
import android.util.SparseArray
import android.view.View
import android.widget.TextView
import org.zclassic.wallet.core.CoreStatus
import org.zclassic.wallet.core.ReadOnlySync
import org.zclassic.wallet.core.Zatoshi

/** Display only: C supplies money formatting and the complete snapshot state.
 * No saved text, endpoint, key, spending action or balance arithmetic. A newly
 * created/restored/detached view has no amount until its live owner updates it.
 */
internal class BalanceView(context: Context) : TextView(context) {
    init {
        id = R.id.address_balance
        isSaveEnabled = false
        setSaveFromParentEnabled(false)
        freezesText = false
        importantForAutofill = View.IMPORTANT_FOR_AUTOFILL_NO_EXCLUDE_DESCENDANTS
        importantForContentCapture = View.IMPORTANT_FOR_CONTENT_CAPTURE_NO_EXCLUDE_DESCENDANTS
        textSize = 18f
        setTextColor(Color.rgb(24, 35, 46))
        setPadding(0, 0, 0, (20 * resources.displayMetrics.density).toInt())
        showUnavailable()
    }

    fun showUnavailable(status: CoreStatus = CoreStatus.OK) {
        text = ""
        text = context.getString(R.string.balance_status_lines,
            context.getString(R.string.balance_unavailable), context.getString(faultText(status)))
    }

    fun show(snapshot: ReadOnlySync.Snapshot) {
        // Clear earlier amounts before any formatting that could fail.
        showUnavailable(snapshot.lastFault)
        val report = snapshot.report
        if (snapshot.freshness == ReadOnlySync.Freshness.UNAVAILABLE || report == null) {
            if (snapshot.refreshing) text = context.getString(R.string.balance_status_lines,
                context.getString(R.string.balance_unavailable), context.getString(R.string.balance_updating))
            return
        }
        val title = if (snapshot.freshness == ReadOnlySync.Freshness.STALE)
            R.string.balance_stale else R.string.balance_unverified
        val lines = mutableListOf(context.getString(title),
            context.getString(R.string.balance_total, report.total.format()),
            context.getString(R.string.balance_confirmed, report.confirmed.format()),
            context.getString(R.string.balance_pending, Zatoshi.formatDelta(report.pendingDelta)),
            context.getString(R.string.balance_report_scope))
        if (snapshot.refreshing) lines.add(context.getString(R.string.balance_updating))
        else if (snapshot.lastFault != CoreStatus.OK) lines.add(context.getString(faultText(snapshot.lastFault)))
        text = lines.joinToString("\n")
    }

    private fun faultText(status: CoreStatus): Int = when (status) {
        CoreStatus.OK -> R.string.balance_not_connected
        CoreStatus.TIMED_OUT -> R.string.balance_timed_out
        CoreStatus.CANCELLED -> R.string.balance_cancelled
        CoreStatus.TLS_FAILURE -> R.string.balance_secure_unavailable
        else -> R.string.balance_update_failed
    }

    // Ignore even an old framework TextView state supplied under this view ID.
    override fun dispatchSaveInstanceState(container: SparseArray<Parcelable>) = Unit
    override fun dispatchRestoreInstanceState(container: SparseArray<Parcelable>) { showUnavailable() }

    override fun onDetachedFromWindow() {
        showUnavailable()
        super.onDetachedFromWindow()
    }
}
