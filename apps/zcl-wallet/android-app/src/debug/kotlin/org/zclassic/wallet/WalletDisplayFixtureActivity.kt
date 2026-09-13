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
    private lateinit var balance: BalanceView
    private lateinit var address: TransparentAddress
    private var sync: ReadOnlySync? = null
    private var presentation: BalancePresentation? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        window.addFlags(WindowManager.LayoutParams.FLAG_SECURE)
        address = TransparentAddress.fromPublicKeyHash(ByteArray(20), Network.MAINNET)
        WalletScreens(this).receive(address, ::finish, ::finish)
        balance = findViewById(R.id.address_balance)
    }

    override fun onResume() {
        super.onResume()
        balance.showUnavailable()
        val next = ReadOnlySync(address, ByteArray(32) { 0x46 }, SystemClock::elapsedRealtime)
        try {
            presentation = BalancePresentation(next, mainExecutor, MainQueueBalanceWakeup(),
                balance::show, balance::showUnavailable)
            sync = next
        } catch (problem: Throwable) {
            next.close()
            throw problem
        }
    }

    internal fun beginFixture(): ReadOnlySync.Attempt = checkNotNull(sync).begin()
    internal fun redrawFixture(): Boolean = checkNotNull(presentation).requestUpdate()

    private fun closeDisplay() {
        val previous = presentation
        val previousSync = sync
        presentation = null
        sync = null
        try { previous?.close() }
        finally {
            previousSync?.close()
            balance.showUnavailable()
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
