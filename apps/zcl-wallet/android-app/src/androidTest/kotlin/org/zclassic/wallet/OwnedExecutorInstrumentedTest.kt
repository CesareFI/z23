// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import androidx.test.ext.junit.runners.AndroidJUnit4
import java.util.concurrent.CountDownLatch
import java.util.concurrent.ThreadFactory
import java.util.concurrent.ThreadPoolExecutor
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicInteger
import java.util.concurrent.atomic.AtomicReference
import org.junit.Assert.assertEquals
import org.junit.Assert.assertSame
import org.junit.Assert.assertThrows
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

/** Controlled thread-factory faults only; no memory exhaustion or wallet state. */
@RunWith(AndroidJUnit4::class)
class OwnedExecutorInstrumentedTest {
    private fun idleBackend(owner: OwnedExecutor, coreSize: Int): ThreadPoolExecutor {
        val thread = AtomicReference<Thread>()
        val ready = CountDownLatch(1)
        assertTrue(owner.submit { thread.set(Thread.currentThread()); ready.countDown() })
        assertTrue(ready.await(5, TimeUnit.SECONDS))
        val field = OwnedExecutor::class.java.getDeclaredField("executor").apply { isAccessible = true }
        val backend = field.get(owner) as ThreadPoolExecutor
        backend.corePoolSize = 0
        val previous = checkNotNull(thread.get())
        previous.join(5000)
        assertTrue(!previous.isAlive)
        backend.corePoolSize = coreSize
        return backend
    }

    @Test fun androidWorkerStartFailureClearsInputAndAllowsAnIndependentRetry() {
        for (coreSize in listOf(0, 1)) verifyFailedHandoff(coreSize)
    }

    @Test fun androidCleanupFailureCannotReplaceTheWorkerStartError() {
        for (coreSize in listOf(0, 1)) verifyFailedHandoff(coreSize, cleanupFailure = true)
        assertBothAdmissionsAvailable()
    }

    @Test fun androidCloseNeedsNoWorkerAndReturnsBothProcessAdmissions() {
        for (coreSize in listOf(0, 1)) {
            for (mode in 0..2) verifyCloseWithoutWorker(coreSize, mode)
        }
    }

    @Test fun androidQueuedCleanupFailureStillRetiresInputsAndSession() {
        for (fatal in listOf(false, true)) verifyQueuedCleanupFailure(fatal)
    }

    @Test fun androidShutdownFailureRetiresAnIdleWorker() {
        verifyShutdownFailure(active = false)
    }

    @Test fun androidShutdownFailureDoesNotInterruptAnActiveOperation() {
        verifyShutdownFailure(active = true)
    }

    private fun verifyShutdownFailure(active: Boolean) {
        val owner = OwnedExecutor()
        val backend = idleBackend(owner, 1)
        val originalFactory = backend.threadFactory
        val problem = OutOfMemoryError("Public shutdown-state allocation failure")
        val armed = AtomicBoolean(false)
        val interrupted = AtomicBoolean(false)
        val workerFailure = AtomicReference<Throwable?>()
        val worker = AtomicReference<Thread>()
        val ready = CountDownLatch(1)
        val release = CountDownLatch(1)
        val finalized = CountDownLatch(1)
        val words = charArrayOf('a', 'b', 'c')
        backend.threadFactory = ThreadFactory { task ->
            object : Thread(task, "WalletPublicShutdownFixture") {
                override fun isInterrupted(): Boolean {
                    if (armed.compareAndSet(true, false)) throw problem
                    return super.isInterrupted()
                }
            }.also { worker.set(it) }
        }
        try {
            assertTrue(owner.submit {
                ready.countDown()
                if (active) try { check(release.await(10, TimeUnit.SECONDS)) }
                catch (failure: Throwable) {
                    interrupted.set(failure is InterruptedException)
                    workerFailure.set(failure)
                }
            })
            assertTrue(ready.await(5, TimeUnit.SECONDS))
            val waiting = if (active) Thread.State.TIMED_WAITING else Thread.State.WAITING
            val limit = System.nanoTime() + TimeUnit.SECONDS.toNanos(5)
            while (worker.get().state != waiting && System.nanoTime() < limit) Thread.yield()
            assertEquals(waiting, worker.get().state)
            armed.set(true)
            assertSame(problem, assertThrows(OutOfMemoryError::class.java) {
                owner.close { words.fill('\u0000'); finalized.countDown() }
            })
            assertTrue(owner.isClosed)
            assertTrue(backend.isShutdown)
            if (active) {
                assertEquals(1L, finalized.count)
                assertTrue(words.contentEquals(charArrayOf('a', 'b', 'c')))
                release.countDown()
            }
            assertTrue("Shutdown failure stranded session cleanup", finalized.await(2, TimeUnit.SECONDS))
            assertTrue(backend.awaitTermination(5, TimeUnit.SECONDS))
            assertTrue(words.all { it == '\u0000' })
            assertTrue(!interrupted.get())
            workerFailure.get()?.let { throw it }
        } finally {
            armed.set(false)
            backend.threadFactory = originalFactory
            release.countDown()
            backend.shutdown()
            assertTrue(backend.awaitTermination(5, TimeUnit.SECONDS))
            owner.close()
            words.fill('\u0000')
        }
        assertBothAdmissionsAvailable()
    }

    private fun verifyQueuedCleanupFailure(fatal: Boolean) {
        val owner = OwnedExecutor()
        val backend = idleBackend(owner, 1)
        val entered = CountDownLatch(1)
        val release = CountDownLatch(1)
        val clears = AtomicInteger()
        val finalCount = AtomicInteger(-1)
        val words = Array(4) { charArrayOf('a', 'b', 'c') }
        val first = if (fatal) OutOfMemoryError("Public queued cleanup failure")
            else IllegalStateException("Public queued cleanup failure")
        val second = IllegalStateException("Public secondary cleanup failure")
        try {
            assertTrue(owner.submit({ clears.incrementAndGet() }) {
                entered.countDown()
                check(release.await(15, TimeUnit.SECONDS))
            })
            assertTrue(entered.await(5, TimeUnit.SECONDS))
            for (index in words.indices) assertTrue(owner.submit({
                words[index].fill('\u0000')
                clears.incrementAndGet()
                if (index == 0) throw first
                if (index == 1) throw second
            }) { error("Cancelled action ran") })
            assertSame(first, assertThrows(Throwable::class.java) {
                owner.close { finalCount.set(clears.get()) }
            })
            assertTrue(owner.isClosed)
            assertEquals(4, clears.get())
            assertTrue(words.all { input -> input.all { it == '\u0000' } })
            assertTrue(backend.queue.isEmpty())
            assertTrue(backend.isShutdown)
            assertEquals(-1, finalCount.get())
            release.countDown()
            assertTrue(backend.awaitTermination(5, TimeUnit.SECONDS))
            assertEquals(5, clears.get())
            assertEquals(5, finalCount.get())
            owner.close { error("Closed owner finalized twice") }
        } finally {
            release.countDown()
            while (true) {
                val task = backend.queue.poll() ?: break
                try { task.run() }
                catch (problem: Throwable) { assertTrue(problem === first || problem === second) }
            }
            backend.shutdown()
            assertTrue(backend.awaitTermination(5, TimeUnit.SECONDS))
            owner.close()
            words.forEach { it.fill('\u0000') }
        }
        assertBothAdmissionsAvailable()
    }

    private fun verifyCloseWithoutWorker(coreSize: Int, mode: Int) {
        val owner = OwnedExecutor()
        val backend = idleBackend(owner, coreSize)
        val originalFactory = backend.threadFactory
        val attempts = AtomicInteger()
        val clears = AtomicInteger()
        val words = charArrayOf('a', 'b', 'c')
        try {
            backend.threadFactory = ThreadFactory {
                attempts.incrementAndGet()
                when (mode) {
                    0 -> null
                    1 -> throw OutOfMemoryError("Synthetic public close allocation failure")
                    else -> throw SecurityException("Synthetic public close thread refusal")
                }
            }
            owner.close { words.fill('\u0000'); clears.incrementAndGet() }
            assertTrue(owner.isClosed)
            assertEquals(0, attempts.get())
            assertEquals(1, clears.get())
            assertTrue(words.all { it == '\u0000' })
            assertTrue(backend.queue.isEmpty())
            assertTrue(backend.awaitTermination(5, TimeUnit.SECONDS))
            owner.close { error("Session finalized twice") }
            assertTrue(!owner.submit { error("Closed task ran") })
        } finally {
            // Bound the public fixture even if the regression fails.
            backend.threadFactory = originalFactory
            while (true) (backend.queue.poll() ?: break).run()
            owner.close()
            backend.shutdown()
            words.fill('\u0000')
            assertTrue(backend.awaitTermination(5, TimeUnit.SECONDS))
        }
        assertEquals(1, clears.get())
        assertBothAdmissionsAvailable()
    }

    private fun assertBothAdmissionsAvailable() {
        val first = OwnedExecutor()
        val second = OwnedExecutor()
        val entered = CountDownLatch(2)
        val release = CountDownLatch(1)
        val pools = mutableListOf<ThreadPoolExecutor>()
        try {
            for (owner in listOf(first, second)) {
                assertTrue(owner.submit {
                    entered.countDown()
                    check(release.await(10, TimeUnit.SECONDS))
                })
                val field = OwnedExecutor::class.java.getDeclaredField("executor").apply { isAccessible = true }
                pools.add(field.get(owner) as ThreadPoolExecutor)
            }
            assertTrue(entered.await(5, TimeUnit.SECONDS))
        } finally {
            release.countDown()
            first.close()
            second.close()
            for (pool in pools) assertTrue(pool.awaitTermination(5, TimeUnit.SECONDS))
        }
    }

    private fun verifyFailedHandoff(coreSize: Int, cleanupFailure: Boolean = false) {
        val owner = OwnedExecutor()
        val backend = idleBackend(owner, coreSize)
        val originalFactory = backend.threadFactory
        val clears = AtomicInteger()
        val ran = CountDownLatch(1)
        val finalized = CountDownLatch(1)
        val words = charArrayOf('a', 'b', 'c')
        val problem = OutOfMemoryError("Synthetic public thread-allocation failure")
        val secondary = IllegalStateException("Public cleanup failure")
        try {
            backend.corePoolSize = coreSize
            backend.threadFactory = ThreadFactory { throw problem }
            val caught = assertThrows(OutOfMemoryError::class.java) {
                owner.submit({
                    words.fill('\u0000')
                    clears.incrementAndGet()
                    if (cleanupFailure) throw secondary
                }) {
                    error("Failed input ran after retry")
                }
            }
            assertSame(problem, caught)
            assertTrue(words.all { it == '\u0000' })
            assertEquals(1, clears.get())
            assertTrue(backend.queue.isEmpty())
            backend.threadFactory = originalFactory
            assertTrue(owner.submit { ran.countDown() })
            assertTrue(ran.await(5, TimeUnit.SECONDS))
        } finally {
            backend.threadFactory = originalFactory
            owner.close { finalized.countDown() }
            assertTrue(finalized.await(5, TimeUnit.SECONDS))
            assertTrue(backend.awaitTermination(5, TimeUnit.SECONDS))
        }
        assertEquals(1, clears.get())
    }
}
