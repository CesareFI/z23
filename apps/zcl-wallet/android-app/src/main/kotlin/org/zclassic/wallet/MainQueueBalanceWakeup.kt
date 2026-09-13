// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.os.Handler
import android.os.Looper
import android.os.SystemClock

/** Handler time schedules only a redraw hint. Freshness/deadlines are always
 * checked again with the sync owner's elapsedRealtime clock, including sleep.
 * Foreground closure cancels this callback; resume creates a new empty owner.
 */
internal class MainQueueBalanceWakeup : BalanceWakeup {
    private val handler = Handler(Looper.getMainLooper())
    private var pending: Runnable? = null

    override fun replace(delayMillis: Long, callback: Runnable): Boolean {
        cancel()
        val uptime = SystemClock.uptimeMillis()
        if (delayMillis <= 0 || uptime < 0 || uptime > Long.MAX_VALUE - delayMillis) return false
        pending = callback
        if (handler.postAtTime(callback, uptime + delayMillis)) return true
        pending = null
        return false
    }

    override fun cancel() {
        check(Looper.myLooper() === handler.looper) { "Balance wakeup requires the main thread" }
        pending?.let(handler::removeCallbacks)
        pending = null
    }
}
