// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import java.util.concurrent.Executor
import java.util.concurrent.RejectedExecutionException
import java.util.concurrent.atomic.AtomicBoolean
import org.zclassic.wallet.core.CoreStatus

/** Owns bounded metadata access and one cancellable wakeup hint from C. Create,
 * deliver and close on the UI thread; producer workers may requestUpdate.
 * Queue only a redraw signal, never a snapshot. No worker, socket, persistence,
 * periodic polling or automatic retry. Normal close requires the caller to
 * clear its views; any delivery failure invokes unavailable after cleanup.
 */
internal class ForegroundPresentation<T>(
    owner: AutoCloseable,
    sample: () -> T,
    nextDelay: (T) -> Long,
    failureStatus: (Exception) -> CoreStatus,
    private val ui: Executor,
    private val wakeup: BalanceWakeup,
    receive: (T) -> Unit,
    unavailable: (CoreStatus) -> Unit
) : AutoCloseable {
    private class Source<T>(val owner: AutoCloseable, val sample: () -> T,
                            val nextDelay: (T) -> Long,
                            val failureStatus: (Exception) -> CoreStatus)
    private class Receiver<T>(val receive: (T) -> Unit, val unavailable: (CoreStatus) -> Unit)
    private class ScheduleFailure : IllegalStateException()
    private val uiThread = Thread.currentThread()
    private val closed = AtomicBoolean(false)
    private val queued = AtomicBoolean(false)
    private var source: Source<T>? = Source(owner, sample, nextDelay, failureStatus)
    private var receiver: Receiver<T>? = Receiver(receive, unavailable)
    private val redraw = Runnable { deliver() }
    private val timedRedraw = Runnable { wakeupFired() }

    /** At most one pending redraw. False means closed or rejected; acceptance
     * does not promise the foreground owner will remain open until delivery.
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
        val currentSource = checkNotNull(source)
        val target = checkNotNull(receiver)
        val snapshot = try {
            wakeup.cancel()
            val current = currentSource.sample()
            val delay = currentSource.nextDelay(current)
            check(delay >= 0) { "Invalid native display wakeup delay" }
            if (delay > 0 && !wakeup.replace(delay, timedRedraw)) throw ScheduleFailure()
            current
        } catch (_: ScheduleFailure) {
            fail(CoreStatus.RESOURCE_EXHAUSTED)
            return
        } catch (problem: Exception) {
            fail(currentSource.failureStatus(problem))
            return
        } catch (problem: Throwable) {
            fail(CoreStatus.IO_UNCERTAIN, problem)
            throw problem
        }
        try { target.receive(snapshot) }
        catch (problem: Throwable) { fail(CoreStatus.IO_UNCERTAIN, problem) }
    }

    private fun fail(status: CoreStatus, original: Throwable? = null) {
        val target = receiver
        var failure = original
        try { close() }
        catch (problem: Throwable) { failure = combine(failure, problem) }
        try { target?.unavailable?.invoke(status) }
        catch (problem: Throwable) { failure = combine(failure, problem) }
        failure?.let { throw it }
    }

    private fun wakeupFired() {
        checkUiThread()
        if (closed.get()) return
        val accepted = try { requestUpdate() }
        catch (problem: Throwable) {
            fail(CoreStatus.IO_UNCERTAIN, problem)
            throw problem
        }
        if (!accepted) fail(CoreStatus.RESOURCE_EXHAUSTED)
    }

    override fun close() {
        checkUiThread()
        if (!closed.compareAndSet(false, true)) return
        queued.set(false)
        val previous = source
        source = null
        receiver = null
        var failure: Throwable? = null
        try { wakeup.cancel() }
        catch (problem: Throwable) { failure = problem }
        try { previous?.owner?.close() }
        catch (problem: Throwable) { failure = combine(failure, problem) }
        failure?.let { throw it }
    }

    private fun checkUiThread() {
        check(Thread.currentThread() === uiThread) { "Foreground presentation requires its UI thread" }
    }

    private fun combine(first: Throwable?, second: Throwable): Throwable {
        if (first == null) return second
        if (first !== second) first.addSuppressed(second)
        return first
    }
}
