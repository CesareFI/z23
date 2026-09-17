// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.os.SystemClock
import org.zclassic.wallet.core.WrappingPolicy

/** One post-authentication unlock, including its worker and UI queue delays.
 * C owns the deadline predicate. Rechecking never extends this origin. */
internal class UnlockWindow(private val nowMillis: () -> Long = SystemClock::elapsedRealtime) {
    private val startedMillis = nowMillis()
    val isOpen: Boolean
        get() = WrappingPolicy.authenticationWindowOpen(startedMillis, nowMillis())

    fun requireOpen() { check(isOpen) { "Wallet unlock expired" } }
}
