// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import org.zclassic.wallet.core.CoreStatus
import org.zclassic.wallet.core.ReadOnlySync

/** Render one foreground snapshot into both views on the UI thread. A failed
 * render leaves neither earlier nor partially updated values on screen.
 * This object owns no sync state, timer, persistence or network capability.
 */
internal class ReadOnlyReportViews(private val balance: BalanceView, private val history: HistoryView) {
    private fun clear() {
        balance.text = ""
        history.text = ""
    }

    fun show(snapshot: ReadOnlySync.Snapshot) {
        clear()
        try {
            balance.show(snapshot)
            history.show(snapshot)
        } catch (problem: Throwable) {
            clear()
            throw problem
        }
    }

    fun showUnavailable(status: CoreStatus = CoreStatus.OK) {
        clear()
        balance.showUnavailable(status)
        history.showUnavailable(status)
    }
}
