// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import java.util.concurrent.ArrayBlockingQueue
import java.util.concurrent.RejectedExecutionException
import java.util.concurrent.ThreadPoolExecutor
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicBoolean

/** Bounded Android platform-work owner. Each submitted input has exactly one
 * cleanup, including rejection/cancellation. close() does not block the UI:
 * it discards queued work, lets the active operation reach its finally block,
 * then clears worker-owned session state on that same thread and shuts down.
 * Cleanup callbacks must only clear owned data and must not throw or block.
 */
internal class OwnedExecutor {
    private val control = Any()
    private val closed = AtomicBoolean(false)
    private val executor = ThreadPoolExecutor(1, 1, 0, TimeUnit.SECONDS, ArrayBlockingQueue(4),
        { task -> Thread(task, "WalletPlatform") }, ThreadPoolExecutor.AbortPolicy())

    val isClosed: Boolean get() = closed.get()

    fun submit(cleanup: () -> Unit = {}, action: () -> Unit): Boolean = synchronized(control) {
        val task = try { OwnedTask(closed, cleanup, action) }
        catch (problem: Throwable) { cleanup(); throw problem }
        if (closed.get()) {
            task.discard()
            return false
        }
        try {
            executor.execute(task)
            true
        } catch (problem: Throwable) {
            // execute may enqueue before starting a worker. A failed handoff
            // must release both its queue slot and its transferred input.
            executor.remove(task)
            task.discard()
            if (problem is RejectedExecutionException) return false
            throw problem
        }
    }

    fun close(clearSession: () -> Unit = {}) = synchronized(control) {
        if (!closed.compareAndSet(false, true)) return
        while (true) {
            val task = executor.queue.poll() ?: break
            (task as OwnedTask).discard()
        }
        // submit/close share this short control lock. The queue is empty and
        // the executor is not shut down, so this finalizer has reserved space.
        executor.execute(clearSession)
        executor.shutdown()
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
