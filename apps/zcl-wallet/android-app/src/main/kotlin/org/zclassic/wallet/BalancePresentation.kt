// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import java.util.concurrent.Executor
import org.zclassic.wallet.core.CoreStatus
import org.zclassic.wallet.core.ReadOnlySync
import org.zclassic.wallet.core.ReadOnlySyncFailure

/** Owns sync until foreground/source/address replacement. Delivery samples C
 * metadata on the UI thread; no network I/O or previously cached snapshot.
 */
internal class BalancePresentation(
    sync: ReadOnlySync,
    ui: Executor,
    wakeup: BalanceWakeup,
    receive: (ReadOnlySync.Snapshot) -> Unit,
    unavailable: (CoreStatus) -> Unit
) : AutoCloseable {
    private val presentation = ForegroundPresentation(sync, sync::snapshot,
        { it.nextChangeDelayMillis },
        { (it as? ReadOnlySyncFailure)?.status ?: CoreStatus.IO_UNCERTAIN },
        ui, wakeup, receive, unavailable)

    fun requestUpdate(): Boolean = presentation.requestUpdate()
    override fun close() = presentation.close()
}
