// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.view.View
import org.zclassic.wallet.core.CoreStatus
import org.zclassic.wallet.core.ReadOnlySync

/** Render one foreground snapshot into both views on the UI thread. A failed
 * render leaves neither earlier nor partially updated values on screen.
 * This object owns no sync state, timer, persistence or network capability.
 */
internal class ReadOnlyReportViews(private val balance: BalanceView, private val history: HistoryView) {
    private fun clear() {
        var failure: Throwable? = null
        try { balance.text = "" }
        catch (problem: Throwable) { failure = problem }
        try { history.text = "" }
        catch (problem: Throwable) {
            val first = failure
            if (first == null) failure = problem
            else if (first !== problem) first.addSuppressed(problem)
        }
        failure?.let { throw it }
    }

    private fun hide() {
        balance.visibility = View.INVISIBLE
        history.visibility = View.INVISIBLE
    }

    private inline fun render(update: () -> Unit) {
        // A platform failure may refuse text clearing itself. Conceal both
        // views first, and reveal them only after the entire update succeeds.
        hide()
        try {
            clear()
            update()
            balance.visibility = View.VISIBLE
            history.visibility = View.VISIBLE
        } catch (problem: Throwable) {
            hide()
            try { clear() }
            catch (cleanup: Throwable) { if (cleanup !== problem) problem.addSuppressed(cleanup) }
            throw problem
        }
    }

    fun show(snapshot: ReadOnlySync.Snapshot) = render {
        balance.show(snapshot)
        history.show(snapshot)
    }

    fun showUnavailable(status: CoreStatus = CoreStatus.OK) = render {
        balance.showUnavailable(status)
        history.showUnavailable(status)
    }
}
