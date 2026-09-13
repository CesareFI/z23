// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.app.Activity
import android.os.Bundle
import android.os.SystemClock
import android.view.WindowManager
import org.zclassic.wallet.core.Network
import org.zclassic.wallet.core.ReadOnlySync
import org.zclassic.wallet.core.TransparentAddress

/** Nonexported debug-only public display fixture. No wallet storage, Keystore,
 * endpoint, input intent, key, secret or automatic fixture replay. Instrumented
 * tests supply public response frames through the normal checked sync attempt.
 * Release source sets do not contain this host.
 */
class WalletDisplayFixtureActivity : Activity() {
    private lateinit var views: ReadOnlyReportViews
    private lateinit var address: TransparentAddress
    private var sync: ReadOnlySync? = null
    private var presentation: BalancePresentation? = null
    private var historyEnabled = false // Deliberately absent from Bundle/intent state.

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        window.addFlags(WindowManager.LayoutParams.FLAG_SECURE)
        address = TransparentAddress.fromPublicKeyHash(ByteArray(20), Network.MAINNET)
        views = WalletScreens(this).receive(address, ::finish, ::finish)
    }

    override fun onResume() {
        super.onResume()
        openDisplay()
    }

    private fun openDisplay() {
        views.showUnavailable()
        val source = ByteArray(32) { 0x46 }
        val next = if (historyEnabled) ReadOnlySync.withHistory(address, source, SystemClock::elapsedRealtime)
            else ReadOnlySync(address, source, SystemClock::elapsedRealtime)
        try {
            presentation = BalancePresentation(next, mainExecutor, MainQueueBalanceWakeup(),
                views::show, views::showUnavailable)
            sync = next
        } catch (problem: Throwable) {
            next.close()
            throw problem
        }
    }

    internal fun beginFixture(): ReadOnlySync.Attempt = checkNotNull(sync).begin()
    internal fun beginHistoryFixture(): ReadOnlySync.Attempt {
        if (!historyEnabled) {
            closeDisplay()
            historyEnabled = true
            openDisplay()
        }
        return beginFixture()
    }
    internal fun redrawFixture(): Boolean = checkNotNull(presentation).requestUpdate()

    private fun closeDisplay() {
        val previous = presentation
        val previousSync = sync
        presentation = null
        sync = null
        try { previous?.close() }
        finally {
            try { previousSync?.close() }
            finally { views.showUnavailable() }
        }
    }

    override fun onPause() {
        closeDisplay()
        super.onPause()
    }

    override fun onDestroy() {
        closeDisplay()
        super.onDestroy()
    }
}
