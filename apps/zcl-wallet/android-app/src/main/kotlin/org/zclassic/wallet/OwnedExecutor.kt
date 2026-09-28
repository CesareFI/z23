// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import java.util.concurrent.ArrayBlockingQueue
import java.util.concurrent.RejectedExecutionException
import java.util.concurrent.Semaphore
import java.util.concurrent.ThreadPoolExecutor
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicBoolean

/** At most two admitted platform-work owners per process, each with one worker
 * and four queued tasks. Closing retains admission until pool termination.
 * Each submitted input has exactly one cleanup, including rejection and
 * cancellation. close() does not block the UI:
 * it discards queued work, lets the active operation reach its finally block,
 * then clears session state after all worker operations have ended. Termination
 * runs cleanup on the last worker, or directly if no worker remains. Cleanup
 * needs no new worker allocation and finishes before admission is released.
 * An owner that never obtained a pool has no worker state; its empty-state
 * finalizer runs directly in close(). Admission never waits or retries itself.
 * Cleanup callbacks must only clear owned data and must not throw or block.
 */
internal class OwnedExecutor {
    private class SessionExecutor(private val control: Any) : ThreadPoolExecutor(1, 1, 0, TimeUnit.SECONDS, ArrayBlockingQueue(4),
        { task -> Thread(task, "WalletPlatform") }, AbortPolicy()) {
        @Volatile var clearSession: (() -> Unit)? = null
        @Volatile private var worker: Thread? = null
        private var executing = false // Guarded by the owner's control monitor.

        override fun beforeExecute(thread: Thread, task: Runnable) {
            super.beforeExecute(thread, task)
            synchronized(control) { worker = thread; executing = true }
        }

        override fun afterExecute(task: Runnable, problem: Throwable?) {
            try { super.afterExecute(task, problem) }
            finally { synchronized(control) { executing = false } }
        }

        fun wakeIdleAfterFailedShutdown() = synchronized(control) {
            // shutdown() can enter SHUTDOWN and fail before waking its idle
            // worker. Active operations (even waiting ones) must finish safely.
            // A task still before beforeExecute will observe the closed owner.
            if (isShutdown && !executing) worker?.interrupt()
        }

        override fun terminated() {
            // ThreadPoolExecutor holds its main lock here. Never take control:
            // close() holds control while acquiring that internal lock.
            worker = null
            val cleanup = clearSession
            clearSession = null
            try { cleanup?.invoke() }
            finally { owners.release() }
        }
    }

    private companion object {
        val owners = Semaphore(2) // Foreground work plus one retiring operation.

        fun reserveExecutor(control: Any): SessionExecutor? {
            if (!owners.tryAcquire()) return null
            var transferred = false
            try {
                val executor = SessionExecutor(control)
                transferred = true
                return executor
            } finally { if (!transferred) owners.release() }
        }
    }

    private val control = Any()
    private val closed = AtomicBoolean(false)
    // Reserve lazily: an abandoned/failed parent constructor cannot leak a slot.
    private var executor: SessionExecutor? = null

    val isClosed: Boolean get() = closed.get()

    fun submit(cleanup: () -> Unit = {}, action: () -> Unit): Boolean = synchronized(control) {
        val task = try { OwnedTask(closed, cleanup, action) }
        catch (problem: Throwable) {
            try { cleanup() } catch (_: Throwable) { /* Preserve task allocation failure. */ }
            throw problem
        }
        if (closed.get()) {
            task.discard()
            return false
        }
        var current: ThreadPoolExecutor? = null
        try {
            current = executor ?: reserveExecutor(control)?.also { executor = it }
            if (current == null) { task.discard(); return false }
            current.execute(task)
            true
        } catch (problem: Throwable) {
            // execute may enqueue before starting a worker. A failed handoff
            // must release both its queue slot and its transferred input.
            current?.remove(task)
            if (problem is RejectedExecutionException) {
                task.discard()
                return false
            }
            // A second cleanup failure must not replace the worker-start error.
            try { task.discard() } catch (_: Throwable) { /* Preserve the first failure. */ }
            throw problem
        }
    }

    fun close(clearSession: () -> Unit = {}) = synchronized(control) {
        if (!closed.compareAndSet(false, true)) return
        val current = executor
        if (current == null) { clearSession(); return }
        var failure: Throwable? = null
        while (true) {
            val task = current.queue.poll() ?: break
            try { (task as OwnedTask).discard() }
            catch (problem: Throwable) { if (failure == null) failure = problem }
        }
        // No new worker or queued finalizer is needed. Shutdown waits for the
        // active task's finally block, then termination owns session cleanup.
        // A broken cleanup callback must not strand the remaining inputs or
        // pool. Preserve the first failure without allocating suppressed data.
        current.clearSession = clearSession
        try { current.shutdown() }
        catch (problem: Throwable) {
            if (failure == null) failure = problem
            try { current.wakeIdleAfterFailedShutdown() }
            catch (_: Throwable) { /* Preserve the first failure. */ }
        }
        if (failure != null) throw failure
    }

    private class OwnedTask(
        private val closed: AtomicBoolean,
        private val cleanup: () -> Unit,
        private val action: () -> Unit,
    ) : Runnable {
        private val claimed = AtomicBoolean(false)

        override fun run() {
            if (!claimed.compareAndSet(false, true)) return
            try {
                if (!closed.get()) action()
            } finally {
                cleanup()
            }
        }

        fun discard() {
            if (claimed.compareAndSet(false, true)) cleanup()
        }
    }
}
