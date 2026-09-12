// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.app.Activity
import android.graphics.Color
import android.view.WindowInsets
import android.widget.Button
import android.widget.LinearLayout
import android.widget.RadioButton
import android.widget.RadioGroup
import android.widget.ScrollView
import android.widget.TextView
import org.zclassic.wallet.core.Network
import org.zclassic.wallet.core.PaymentRequest

internal class ScanScreens(private val activity: Activity) {
    private val root = LinearLayout(activity).apply {
        orientation = LinearLayout.VERTICAL
        isSaveEnabled = false
        setBackgroundColor(Color.WHITE)
    }
    private val spacing = (16 * activity.resources.displayMetrics.density).toInt()
    private var preview: CameraPreviewView? = null

    init {
        root.setOnApplyWindowInsetsListener { _, insets ->
            val bars = insets.getInsets(WindowInsets.Type.systemBars() or WindowInsets.Type.displayCutout())
            root.setPadding(spacing + bars.left, spacing + bars.top, spacing + bars.right, spacing + bars.bottom)
            insets
        }
        activity.setContentView(root)
    }

    fun clear() {
        preview?.clear()
        preview = null
        root.removeAllViews()
    }

    private fun text(parent: LinearLayout, value: String, identifier: Int = 0, size: Float = 17f) = TextView(activity).apply {
        text = value
        if (identifier != 0) id = identifier
        textSize = size
        setTextColor(Color.rgb(24, 35, 46))
        setPadding(0, 0, 0, spacing)
        isSaveEnabled = false
        parent.addView(this)
    }

    private fun button(parent: LinearLayout, label: Int, identifier: Int, action: () -> Unit) = Button(activity).apply {
        id = identifier
        setText(label)
        filterTouchesWhenObscured = true
        isSaveEnabled = false
        setOnClickListener { action() }
        parent.addView(this)
    }

    private fun begin(message: Int) {
        clear()
        text(root, activity.getString(R.string.scan_request), R.id.screen_title, 26f)
        text(root, activity.getString(message), R.id.status_message)
    }

    private fun networkLabel(network: Network) = activity.getString(
        if (network == Network.MAINNET) R.string.network_mainnet else R.string.network_testnet)

    fun choose(network: Network, message: Int, selected: (Network) -> Unit,
               start: (Network) -> Unit, close: () -> Unit) {
        begin(message)
        val group = RadioGroup(activity)
        for (chain in listOf(Network.TESTNET, Network.MAINNET)) group.addView(RadioButton(activity).apply {
            id = if (chain == Network.MAINNET) R.id.network_mainnet else R.id.network_testnet
            text = networkLabel(chain)
            isSaveEnabled = false
            filterTouchesWhenObscured = true
        })
        group.check(if (network == Network.MAINNET) R.id.network_mainnet else R.id.network_testnet)
        group.setOnCheckedChangeListener { _, identifier ->
            when (identifier) {
                R.id.network_mainnet -> selected(Network.MAINNET)
                R.id.network_testnet -> selected(Network.TESTNET)
            }
        }
        root.addView(group)
        button(root, R.string.scan_start, R.id.scan_start) {
            start(if (group.checkedRadioButtonId == R.id.network_mainnet) Network.MAINNET else Network.TESTNET)
        }
        button(root, R.string.scan_close, R.id.scan_close, close)
    }

    fun scanning(network: Network, cancel: () -> Unit) {
        begin(R.string.scan_position)
        text(root, networkLabel(network))
        val view = CameraPreviewView(activity)
        preview = view
        root.addView(view, LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, 0, 1f))
        button(root, R.string.scan_cancel, R.id.scan_cancel, cancel)
    }

    fun frame(packet: ByteArray, orientation: Int, front: Boolean) {
        preview?.show(packet, orientation, front)
    }

    fun review(request: PaymentRequest, again: () -> Unit, close: () -> Unit) {
        begin(R.string.scan_review_notice)
        val body = LinearLayout(activity).apply { orientation = LinearLayout.VERTICAL; isSaveEnabled = false }
        root.addView(ScrollView(activity).apply { isSaveEnabled = false; addView(body) },
            LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, 0, 1f))
        text(body, networkLabel(request.address.network))
        text(body, request.address.encoded, R.id.scan_address, 21f)
        text(body, request.amount?.let { activity.getString(R.string.scan_amount, it.format()) }
            ?: activity.getString(R.string.scan_no_amount), R.id.scan_amount)
        request.label?.let { text(body, activity.getString(R.string.scan_label, it), R.id.scan_label) }
        request.message?.let { text(body, activity.getString(R.string.scan_message, it), R.id.scan_message) }
        button(root, R.string.scan_again, R.id.scan_start, again)
        button(root, R.string.scan_close, R.id.scan_close, close)
    }
}
