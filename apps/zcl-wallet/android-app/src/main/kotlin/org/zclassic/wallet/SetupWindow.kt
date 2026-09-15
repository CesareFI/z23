// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.os.SystemClock
import org.zclassic.wallet.core.WrappingPolicy

/** Immutable public clock origin shared by one setup's UI and worker. Create
 * after prompt approval and before secret work; retries retain this origin.
 * C decides expiry. Handler timers only schedule cleanup when queues can run. */
internal class SetupWindow(private val nowMillis: () -> Long = SystemClock::elapsedRealtime) {
    private val startedMillis = nowMillis()
    val remainingMillis: Long
        get() = WrappingPolicy.setupWindowRemainingMillis(startedMillis, nowMillis())

    fun requireOpen() { check(remainingMillis > 0) { "Wallet setup expired" } }
}
