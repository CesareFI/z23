// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import java.util.concurrent.Executor
import java.util.concurrent.RejectedExecutionException
import java.util.concurrent.atomic.AtomicBoolean
import org.zclassic.wallet.core.CoreStatus
import org.zclassic.wallet.core.ReadOnlySync
import org.zclassic.wallet.core.ReadOnlySyncFailure

/** Owns sync until foreground/source/address replacement. Create, deliver and
 * close on the UI thread; requestUpdate may run on the producer worker. Queue
 * only a redraw signal, never a previously sampled snapshot. This query does
 * bounded C metadata work; neither the owner lock nor UI may span network I/O.
 * No timer, worker, socket, persistence or automatic retry is created here.
 */
internal class BalancePresentation(
    private val sync: ReadOnlySync,
    private val ui: Executor,
    receive: (ReadOnlySync.Snapshot) -> Unit,
    unavailable: (CoreStatus) -> Unit
) : AutoCloseable {
    private class Receiver(val receive: (ReadOnlySync.Snapshot) -> Unit,
                           val unavailable: (CoreStatus) -> Unit)
    private val uiThread = Thread.currentThread()
    private val closed = AtomicBoolean(false)
    private val queued = AtomicBoolean(false)
    private var receiver: Receiver? = Receiver(receive, unavailable)
    private val redraw = Runnable { deliver() }

    /** Coalesces at most one pending redraw. False means closed or rejected;
     * acceptance is not a promise that a foreground session stays open.
     */
    fun requestUpdate(): Boolean {
        if (closed.get()) return false
        if (!queued.compareAndSet(false, true)) return !closed.get()
        if (closed.get()) { queued.set(false); return false }
        try {
            ui.execute(redraw)
            return true
        } catch (_: RejectedExecutionException) {
            queued.set(false)
            return false
        } catch (problem: Throwable) {
            queued.set(false)
            throw problem
        }
    }

    private fun deliver() {
        checkUiThread()
        if (!queued.getAndSet(false) || closed.get()) return
        val target = checkNotNull(receiver)
        val snapshot = try {
            sync.snapshot()
        } catch (problem: ReadOnlySyncFailure) {
            fail(problem.status)
            return
        } catch (_: Exception) {
            fail(CoreStatus.IO_UNCERTAIN)
            return
        } catch (problem: Throwable) {
            close()
            throw problem
        }
        try {
            target.receive(snapshot)
        } catch (problem: Throwable) {
            close()
            throw problem
        }
    }

    private fun fail(status: CoreStatus) {
        val target = receiver
        close()
        target?.unavailable?.invoke(status)
    }

    override fun close() {
        checkUiThread()
        if (!closed.compareAndSet(false, true)) return
        queued.set(false)
        receiver = null
        sync.close()
    }

    private fun checkUiThread() {
        check(Thread.currentThread() === uiThread) { "Balance presentation requires its UI thread" }
    }
}
