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

/** Public display only. C supplies bounded server assertions and freshness.
 * No saved transaction IDs, external links, amounts or confirmation claims.
 */
internal class HistoryView(context: Context) : TextView(context) {
    init {
        id = R.id.address_history
        isSaveEnabled = false
        setSaveFromParentEnabled(false)
        freezesText = false
        importantForAutofill = View.IMPORTANT_FOR_AUTOFILL_NO_EXCLUDE_DESCENDANTS
        importantForContentCapture = View.IMPORTANT_FOR_CONTENT_CAPTURE_NO_EXCLUDE_DESCENDANTS
        textSize = 16f
        setTextColor(Color.rgb(24, 35, 46))
        setPadding(0, 0, 0, (20 * resources.displayMetrics.density).toInt())
        showUnavailable()
    }

    fun showUnavailable(status: CoreStatus = CoreStatus.OK) {
        text = ""
        text = context.getString(R.string.history_status_lines,
            context.getString(R.string.history_unavailable), context.getString(faultText(status)))
    }

    fun show(snapshot: ReadOnlySync.Snapshot) {
        // Clear earlier IDs before any validation/formatting/allocation can fail.
        showUnavailable(snapshot.lastFault)
        val report = snapshot.report
        val history = report?.history
        if (snapshot.freshness == ReadOnlySync.Freshness.UNAVAILABLE || history == null) {
            if (snapshot.refreshing) text = context.getString(R.string.history_status_lines,
                context.getString(R.string.history_unavailable), context.getString(R.string.history_waiting))
            else if (report != null && snapshot.lastFault == CoreStatus.OK) text = context.getString(R.string.history_status_lines,
                context.getString(R.string.history_unavailable), context.getString(R.string.history_not_reported))
            return
        }
        require(history.size <= 16) { "Invalid history display size" }
        val title = if (snapshot.freshness == ReadOnlySync.Freshness.STALE)
            R.string.history_stale else R.string.history_unverified
        val lines = mutableListOf(context.getString(title), context.getString(R.string.history_scope))
        if (history.isEmpty()) lines.add(context.getString(R.string.history_empty_assertion))
        for (entry in history) lines.add(entryText(entry))
        if (snapshot.refreshing) lines.add(context.getString(R.string.history_waiting))
        else if (snapshot.lastFault != CoreStatus.OK) lines.add(context.getString(faultText(snapshot.lastFault)))
        text = lines.joinToString("\n\n")
    }

    private fun entryText(entry: ReadOnlySync.HistoryEntry): String {
        require(entry.transactionId.length == 64 && entry.transactionId.all {
            it in '0'..'9' || it in 'a'..'f'
        }) { "Invalid history display ID" }
        require(entry.reportedHeight in -1L..Int.MAX_VALUE.toLong()) { "Invalid history display height" }
        val status = when (entry.reportedHeight) {
            -1L -> context.getString(R.string.history_pending_parent)
            0L -> context.getString(R.string.history_pending)
            else -> context.getString(R.string.history_reported_height, entry.reportedHeight)
        }
        return context.getString(R.string.history_status_lines, entry.transactionId, status)
    }

    private fun faultText(status: CoreStatus): Int = when (status) {
        CoreStatus.OK -> R.string.balance_not_connected
        CoreStatus.TIMED_OUT -> R.string.history_timed_out
        CoreStatus.CANCELLED -> R.string.history_cancelled
        CoreStatus.TLS_FAILURE -> R.string.balance_secure_unavailable
        else -> R.string.history_update_failed
    }

    override fun dispatchSaveInstanceState(container: SparseArray<Parcelable>) = Unit
    override fun dispatchRestoreInstanceState(container: SparseArray<Parcelable>) { showUnavailable() }

    override fun onDetachedFromWindow() {
        showUnavailable()
        super.onDetachedFromWindow()
    }
}
