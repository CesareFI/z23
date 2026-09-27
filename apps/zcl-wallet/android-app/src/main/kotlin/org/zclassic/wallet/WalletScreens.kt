// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.app.Activity
import android.graphics.Color
import android.view.WindowInsets
import android.widget.Button
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.RadioButton
import android.widget.RadioGroup
import android.widget.ScrollView
import android.widget.TextView
import org.zclassic.wallet.core.Network
import org.zclassic.wallet.core.ReceiveQr
import org.zclassic.wallet.core.TransparentAddress

/** Android views only. Secret arrays have one view owner and are cleared on
 * every screen transition; no phrase is placed in saved state, an IME or logs.
 */
internal class WalletScreens(private val activity: Activity) {
    private val spacing = (20 * activity.resources.displayMetrics.density).toInt()
    private val content = LinearLayout(activity).apply {
        orientation = LinearLayout.VERTICAL
        isSaveEnabled = false
        setPadding(spacing, spacing, spacing, spacing)
    }
    private val scroll = ScrollView(activity).apply {
        isFillViewport = true
        isSaveEnabled = false
        addView(content)
    }
    // Keep system bars outside the scroller's viewport, including when Android
    // scrolls a requested control into view.
    private val root = FrameLayout(activity).apply {
        isSaveEnabled = false
        setBackgroundColor(Color.WHITE)
        addView(scroll, FrameLayout.LayoutParams(FrameLayout.LayoutParams.MATCH_PARENT,
            FrameLayout.LayoutParams.MATCH_PARENT))
    }
    private var recoveryWords: RecoveryWordsView? = null
    private var recoveryInput: RecoveryInputView? = null

    init {
        root.setOnApplyWindowInsetsListener { _, insets ->
            val bars = insets.getInsets(WindowInsets.Type.systemBars() or WindowInsets.Type.displayCutout())
            root.setPadding(bars.left, bars.top, bars.right, bars.bottom)
            insets
        }
        activity.setContentView(root)
    }

    fun clearSecrets() {
        recoveryWords?.clearSecret()
        recoveryInput?.clearSecret()
        recoveryWords = null
        recoveryInput = null
    }

    private fun begin(title: Int, message: Int) {
        clearSecrets()
        content.removeAllViews()
        text(activity.getString(R.string.app_name), 15f)
        text(activity.getString(title), 28f).id = R.id.screen_title
        text(activity.getString(R.string.development_notice), 14f)
        text(activity.getString(message), 17f).id = R.id.status_message
        scroll.scrollTo(0, 0)
    }

    private fun text(value: String, size: Float): TextView = TextView(activity).apply {
        text = value // Only public UI/address text enters this helper.
        textSize = size
        setTextColor(Color.rgb(24, 35, 46))
        setPadding(0, 0, 0, spacing)
        isSaveEnabled = false
        content.addView(this)
    }

    private fun button(label: Int, identifier: Int, action: () -> Unit) = Button(activity).apply {
        id = identifier
        setText(label)
        filterTouchesWhenObscured = true
        isSaveEnabled = false
        setOnClickListener { action() }
        content.addView(this)
    }

    fun waiting(message: Int = R.string.wallet_working) = begin(R.string.wallet_title, message)

    fun welcome(create: (Network) -> Unit, restore: (Network) -> Unit, scan: (Network) -> Unit) {
        begin(R.string.wallet_welcome, R.string.wallet_setup_description)
        val networks = RadioGroup(activity)
        for ((network, label) in listOf(Network.TESTNET to R.string.network_testnet,
                                       Network.MAINNET to R.string.network_mainnet)) {
            networks.addView(RadioButton(activity).apply {
                id = if (network == Network.TESTNET) R.id.network_testnet else R.id.network_mainnet
                setText(label)
                filterTouchesWhenObscured = true
                isSaveEnabled = false
            })
        }
        networks.check(R.id.network_testnet)
        content.addView(networks)
        fun selected() = if (networks.checkedRadioButtonId == R.id.network_mainnet)
            Network.MAINNET else Network.TESTNET
        button(R.string.create_wallet, R.id.create_wallet) { create(selected()) }
        button(R.string.restore_wallet, R.id.restore_wallet) { restore(selected()) }
        button(R.string.scan_request, R.id.scan_request) { scan(selected()) }
    }

    fun locked(pending: Boolean, unlock: () -> Unit) {
        begin(R.string.wallet_locked, if (pending) R.string.wallet_pending else R.string.wallet_saved)
        button(R.string.unlock_wallet, R.id.unlock_wallet, unlock)
    }

    fun backup(ownedWords: CharArray, next: () -> Unit, cancel: () -> Unit) {
        try {
            begin(R.string.backup_title, R.string.backup_description)
            val view = RecoveryWordsView(activity)
            recoveryWords = view
            view.id = R.id.recovery_words
            content.addView(view)
            view.show(ownedWords)
            button(R.string.backup_written, R.id.backup_written, next)
            button(R.string.cancel_setup, R.id.cancel_setup, cancel)
        } catch (problem: Throwable) {
            // Ownership starts at entry. A later control/layout failure must
            // also conceal any framework copy already displayed by the view.
            ownedWords.fill('\u0000')
            try { clearSecrets() }
            catch (cleanup: Throwable) { if (cleanup !== problem) problem.addSuppressed(cleanup) }
            throw problem
        }
    }

    fun enterRecovery(confirming: Boolean, retry: Boolean, submit: (CharArray) -> Unit, cancel: () -> Unit) {
        val title = if (confirming) R.string.confirm_backup_title else R.string.restore_title
        val message = when {
            retry && confirming -> R.string.confirm_backup_mismatch
            retry -> R.string.restore_invalid
            confirming -> R.string.confirm_backup_description
            else -> R.string.restore_description
        }
        begin(title, message)
        val input = RecoveryInputView(activity)
        recoveryInput = input
        input.id = R.id.recovery_input
        content.addView(input)
        button(R.string.save_wallet, R.id.save_wallet) {
            val owned = input.takeInput()
            var transferred = false
            try {
                submit(owned)
                transferred = true
            } finally {
                if (!transferred) owned.fill('\u0000')
            }
        }
        button(R.string.cancel_setup, R.id.cancel_setup, cancel)
    }

    fun receive(address: TransparentAddress, lock: () -> Unit, scan: () -> Unit): ReadOnlyReportViews {
        begin(R.string.receive_title, R.string.receiving_description)
        text(activity.getString(if (address.network == Network.MAINNET)
            R.string.network_mainnet else R.string.network_testnet), 16f)
        text(address.encoded, 21f).apply {
            id = R.id.receiving_address
            setTextIsSelectable(true) // Public address only.
        }
        val qr = runCatching { ReceiveQr.forAddress(address) }.getOrNull()
        if (qr != null) content.addView(ReceiveQrView(activity).apply { show(qr) })
        else text(activity.getString(R.string.receiving_qr_unavailable), 16f)
        val balance = BalanceView(activity)
        content.addView(balance) // No qualified network source is enabled.
        button(R.string.lock_wallet, R.id.lock_wallet, lock)
        button(R.string.scan_request, R.id.scan_request, scan)
        val history = HistoryView(activity)
        content.addView(history) // Empty until a qualified source is available.
        return ReadOnlyReportViews(balance, history)
    }

    fun failure(message: Int, retry: () -> Unit) {
        begin(R.string.wallet_unavailable, message)
        button(R.string.retry_wallet, R.id.retry_wallet, retry)
    }

    fun review(close: () -> Unit): ReviewView {
        begin(R.string.review_title, R.string.review_description)
        val view = ReviewView(activity)
        content.addView(view)
        button(R.string.review_close, R.id.review_close, close)
        return view
    }
}
