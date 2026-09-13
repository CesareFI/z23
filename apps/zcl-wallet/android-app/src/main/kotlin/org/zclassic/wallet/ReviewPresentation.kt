// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import java.util.concurrent.Executor
import org.zclassic.wallet.core.CoreStatus
import org.zclassic.wallet.core.UnsignedReview
import org.zclassic.wallet.core.UnsignedReviewFailure

/** Owns an already prepared unsigned review until background, lock or screen
 * replacement. Every UI delivery checks the original C lifetime. A snapshot
 * is display data, never consent or signing authority. Never restore this owner.
 */
internal class ReviewPresentation(
    review: UnsignedReview,
    ui: Executor,
    wakeup: BalanceWakeup,
    receive: (UnsignedReview.Snapshot) -> Unit,
    unavailable: (CoreStatus) -> Unit
) : AutoCloseable {
    private val presentation = ForegroundPresentation(review, review::snapshot,
        { it.remainingMillis },
        { (it as? UnsignedReviewFailure)?.status ?: CoreStatus.IO_UNCERTAIN },
        ui, wakeup, receive, unavailable)

    fun requestUpdate(): Boolean = presentation.requestUpdate()
    override fun close() = presentation.close()
}
