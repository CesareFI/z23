// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.app.Activity
import android.os.Bundle
import android.os.Looper
import android.view.WindowManager
import org.zclassic.wallet.core.UnsignedReview

/** Nonexported debug host, absent from release. Instrumentation prepares public
 * reviews off the UI thread and explicitly transfers them here. No wallet,
 * Keystore, endpoint, key, fixture asset, intent data or automatic replay.
 */
class WalletReviewFixtureActivity : Activity() {
    private lateinit var view: ReviewView
    private var presentation: ReviewPresentation? = null
    private var foreground = false

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        window.addFlags(WindowManager.LayoutParams.FLAG_SECURE)
        view = WalletScreens(this).review(::finish)
    }

    override fun onResume() {
        super.onResume()
        foreground = true
        view.showUnavailable()
    }

    /** UI-thread ownership transfer, including refusal/failure. A late prepared
     * owner is cancelled immediately; it cannot survive in the native BUSY slot.
     */
    internal fun showFixture(ownedReview: UnsignedReview): Boolean {
        check(Looper.myLooper() === mainLooper)
        try {
            if (!foreground || isFinishing || isDestroyed) {
                ownedReview.close()
                return false
            }
            closeFixture()
            val next = ReviewPresentation(ownedReview, mainExecutor, MainQueueBalanceWakeup(),
                view::show, view::showUnavailable)
            presentation = next
            if (next.requestUpdate()) return true
            closeFixture()
            return false
        } catch (problem: Throwable) {
            try { closeFixture() }
            catch (cleanup: Throwable) { if (problem !== cleanup) problem.addSuppressed(cleanup) }
            try { ownedReview.close() }
            catch (cleanup: Throwable) { if (problem !== cleanup) problem.addSuppressed(cleanup) }
            throw problem
        }
    }

    internal fun closeFixture() {
        val previous = presentation
        presentation = null
        try { previous?.close() }
        finally { view.showUnavailable() }
    }

    override fun onPause() {
        foreground = false
        try { closeFixture() }
        finally { super.onPause() }
    }

    override fun onDestroy() {
        foreground = false
        try { closeFixture() }
        finally { super.onDestroy() }
    }
}
