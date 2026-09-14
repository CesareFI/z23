// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.content.Context
import android.graphics.Color
import android.os.Parcelable
import android.util.SparseArray
import android.view.View
import android.widget.TextView
import org.zclassic.wallet.core.CoreStatus
import org.zclassic.wallet.core.Network
import org.zclassic.wallet.core.UnsignedReview

/** Displays one live C review snapshot. C supplies accounting and destination
 * encoding; no ownership, finality, consent or signing decision is made here.
 * A restored or detached view retains no review text. The foreground owner
 * must clear the view on close and re-read C before every queued delivery.
 */
internal class ReviewView(context: Context) : TextView(context) {
    init {
        id = R.id.transaction_review
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

    private inline fun replaceText(render: () -> String) {
        // Clearing itself may be refused by Android. Retained or partially
        // replaced text must stay concealed until a complete update succeeds.
        visibility = INVISIBLE
        try {
            text = ""
            text = render()
            visibility = VISIBLE
        } catch (problem: Throwable) {
            try { visibility = INVISIBLE; text = "" }
            catch (cleanup: Throwable) { if (cleanup !== problem) problem.addSuppressed(cleanup) }
            throw problem
        }
    }

    fun showUnavailable(status: CoreStatus = CoreStatus.OK) = replaceText {
        val reason = when (status) {
            CoreStatus.OK -> R.string.review_waiting
            CoreStatus.TIMED_OUT -> R.string.review_expired
            CoreStatus.CANCELLED -> R.string.review_cancelled
            else -> R.string.review_failed
        }
        context.getString(R.string.review_status_lines,
            context.getString(R.string.review_unavailable), context.getString(reason))
    }

    fun show(snapshot: UnsignedReview.Snapshot) {
        // Never retain a previous review if any later conversion fails.
        showUnavailable()
        require(snapshot.inputs.size in 1..8 && snapshot.outputs.size in 1..16)
        require(snapshot.serializedSize in 1..1925 && snapshot.remainingMillis in 1L..90_000L)
        val lines = mutableListOf(context.getString(R.string.review_unsigned),
            context.getString(if (snapshot.network == Network.MAINNET)
                R.string.network_mainnet else R.string.network_testnet),
            context.getString(R.string.review_funding_notice),
            context.getString(R.string.review_sending_notice))
        snapshot.outputs.forEachIndexed { index, destination ->
            lines.add(context.getString(R.string.review_output, index + 1,
                address(destination, snapshot.network), destination.value.format()))
        }
        lines.add(context.getString(R.string.review_input_total, snapshot.inputTotal.format()))
        lines.add(context.getString(R.string.review_output_total, snapshot.outputTotal.format()))
        lines.add(context.getString(R.string.review_fee, snapshot.fee.format()))
        lines.add(context.getString(R.string.review_fee_limit, snapshot.maximumFee.format()))
        lines.add(context.getString(R.string.review_details))
        lines.add(context.getString(R.string.review_draft_id, hash(snapshot.transactionId)))
        lines.add(context.getString(R.string.review_size, snapshot.serializedSize))
        lines.add(context.getString(R.string.review_lock_time, uint32(snapshot.lockTime)))
        lines.add(context.getString(R.string.review_expiry_height, uint32(snapshot.expiryHeight)))
        snapshot.inputs.forEachIndexed { index, input ->
            lines.add(context.getString(R.string.review_input, index + 1,
                hash(input.previousTransactionId), uint32(input.previousIndex), uint32(input.sequence),
                address(input.destination, snapshot.network), input.destination.value.format()))
        }
        replaceText { lines.joinToString("\n\n") }
    }

    private fun address(destination: UnsignedReview.Destination, network: Network): String {
        require(destination.network == network) { "Review destination network mismatch" }
        return destination.address().encoded
    }

    private fun hash(value: String): String {
        require(value.length == 64 && value.all { it in '0'..'9' || it in 'a'..'f' })
        return value
    }

    private fun uint32(value: Long): String {
        require(value in 0..0xffff_ffffL)
        return value.toString()
    }

    override fun dispatchSaveInstanceState(container: SparseArray<Parcelable>) = Unit
    override fun dispatchRestoreInstanceState(container: SparseArray<Parcelable>) { showUnavailable() }

    override fun onDetachedFromWindow() {
        showUnavailable()
        super.onDetachedFromWindow()
    }
}
