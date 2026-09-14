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
 * then clears worker-owned session state on that same thread and shuts down.
 * An owner that never obtained a pool has no worker state; its empty-state
 * finalizer runs directly in close(). Admission never waits or retries itself.
 * Cleanup callbacks must only clear owned data and must not throw or block.
 */
internal class OwnedExecutor {
    private companion object {
        val owners = Semaphore(2) // Foreground work plus one retiring operation.

        fun reserveExecutor(): ThreadPoolExecutor? {
            if (!owners.tryAcquire()) return null
            var transferred = false
            try {
                val executor = object : ThreadPoolExecutor(1, 1, 0, TimeUnit.SECONDS, ArrayBlockingQueue(4),
                    { task -> Thread(task, "WalletPlatform") }, AbortPolicy()) {
                    override fun terminated() { owners.release() }
                }
                transferred = true
                return executor
            } finally { if (!transferred) owners.release() }
        }
    }

    private val control = Any()
    private val closed = AtomicBoolean(false)
    // Reserve lazily: an abandoned/failed parent constructor cannot leak a slot.
    private var executor: ThreadPoolExecutor? = null

    val isClosed: Boolean get() = closed.get()

    fun submit(cleanup: () -> Unit = {}, action: () -> Unit): Boolean = synchronized(control) {
        val task = try { OwnedTask(closed, cleanup, action) }
        catch (problem: Throwable) { cleanup(); throw problem }
        if (closed.get()) {
            task.discard()
            return false
        }
        var current: ThreadPoolExecutor? = null
        try {
            current = executor ?: reserveExecutor()?.also { executor = it }
            if (current == null) { task.discard(); return false }
            current.execute(task)
            true
        } catch (problem: Throwable) {
            // execute may enqueue before starting a worker. A failed handoff
            // must release both its queue slot and its transferred input.
            current?.remove(task)
            task.discard()
            if (problem is RejectedExecutionException) return false
            throw problem
        }
    }

    fun close(clearSession: () -> Unit = {}) = synchronized(control) {
        if (!closed.compareAndSet(false, true)) return
        val current = executor
        if (current == null) { clearSession(); return }
        while (true) {
            val task = current.queue.poll() ?: break
            (task as OwnedTask).discard()
        }
        // submit/close share this short control lock. The queue is empty and
        // the executor is not shut down, so this finalizer has reserved space.
        try { current.execute(clearSession) }
        finally { current.shutdown() }
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
