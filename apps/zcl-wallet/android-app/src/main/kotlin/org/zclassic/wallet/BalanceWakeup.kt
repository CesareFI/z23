// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

/** One owned delayed UI callback. Operations and delivery use the UI thread.
 * replace cancels any earlier callback; cancellation is idempotent. The callback
 * may be late and must re-read C state. No periodic polling or network retry.
 */
internal interface BalanceWakeup {
    fun replace(delayMillis: Long, callback: Runnable): Boolean
    fun cancel()
}
