// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import java.util.concurrent.ThreadFactory
import java.util.concurrent.ThreadPoolExecutor
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicInteger
import java.util.concurrent.atomic.AtomicReference
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertFailsWith
import kotlin.test.assertSame
import kotlin.test.assertTrue

class OwnedExecutorTest {
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
        assertFalse(previous.isAlive)
        backend.corePoolSize = coreSize
        return backend
    }

    private fun awaitClosed(owner: OwnedExecutor) {
        val field = OwnedExecutor::class.java.getDeclaredField("executor").apply { isAccessible = true }
        val backend = field.get(owner) as ThreadPoolExecutor?
        if (backend != null) assertTrue(backend.awaitTermination(5, TimeUnit.SECONDS))
    }

    @Test fun shutdownFailureRetiresAnIdleWorker() {
        verifyShutdownFailure(active = false)
    }

    @Test fun shutdownFailureDoesNotInterruptAnActiveOperation() {
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
            assertSame(problem, assertFailsWith<OutOfMemoryError> {
                owner.close { words.fill('\u0000'); finalized.countDown() }
            })
            assertTrue(owner.isClosed)
            assertTrue(backend.isShutdown)
            if (active) {
                assertEquals(1L, finalized.count)
                assertTrue(words.contentEquals(charArrayOf('a', 'b', 'c')))
                release.countDown()
            }
            assertTrue(finalized.await(2, TimeUnit.SECONDS), "Shutdown failure stranded session cleanup")
            assertTrue(backend.awaitTermination(5, TimeUnit.SECONDS))
            assertTrue(words.all { it == '\u0000' })
            assertFalse(interrupted.get())
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

    @Test fun closingWorkersRetainTheProcessBudgetUntilTheirTasksFinish() {
        val first = OwnedExecutor()
        val second = OwnedExecutor()
        val entered = CountDownLatch(2)
        val release = CountDownLatch(1)
        val finalizing = CountDownLatch(2)
        val finishCleanup = CountDownLatch(1)
        val timedOut = AtomicInteger()
        val clears = AtomicInteger()
        var refused: OwnedExecutor? = null
        try {
            for (owner in listOf(first, second)) {
                assertTrue(owner.submit {
                    entered.countDown()
                    if (!release.await(15, TimeUnit.SECONDS)) timedOut.incrementAndGet()
                })
            }
            assertTrue(entered.await(5, TimeUnit.SECONDS))
            for (owner in listOf(first, second)) owner.close {
                // Controlled public fixture; production cleanup never blocks.
                finalizing.countDown()
                if (!finishCleanup.await(15, TimeUnit.SECONDS)) timedOut.incrementAndGet()
            }
            val third = OwnedExecutor().also { refused = it }
            val words = charArrayOf('a', 'b', 'c')
            try {
                assertFalse(third.submit({ words.fill('\u0000'); clears.incrementAndGet() }) {})
                assertTrue(words.all { it == '\u0000' })
                assertEquals(1, clears.get())
                release.countDown()
                assertTrue(finalizing.await(5, TimeUnit.SECONDS))
                assertFalse(third.submit({ clears.incrementAndGet() }) {})
                assertEquals(2, clears.get())
            } finally { words.fill('\u0000') }
        } finally {
            refused?.close()
            first.close()
            second.close()
            release.countDown()
            finishCleanup.countDown()
            awaitClosed(first)
            awaitClosed(second)
            refused?.let(::awaitClosed)
        }
        assertEquals(0, timedOut.get())
        val replacement = OwnedExecutor()
        val ran = CountDownLatch(1)
        try {
            assertTrue(replacement.submit { ran.countDown() })
            assertTrue(ran.await(5, TimeUnit.SECONDS))
        } finally { replacement.close(); awaitClosed(replacement) }
    }

    @Test fun threadCreationFailureClearsTransferredInputAndPreservesFailure() {
        val owner = OwnedExecutor()
        val backend = idleBackend(owner, 1)
        val originalFactory = backend.threadFactory
        val cleared = AtomicInteger()
        val finalized = CountDownLatch(1)
        val problem = OutOfMemoryError("Synthetic public thread-allocation failure")
        val words = charArrayOf('a', 'b', 'c')
        try {
            backend.threadFactory = ThreadFactory { throw problem }
            assertSame(problem, assertFailsWith<OutOfMemoryError> {
                owner.submit({ words.fill('\u0000'); cleared.incrementAndGet() }) {
                    error("Failed submission ran")
                }
            })
            assertTrue(words.all { it == '\u0000' })
            assertEquals(1, cleared.get())
            assertTrue(backend.queue.isEmpty())
        } finally {
            backend.threadFactory = originalFactory
            owner.close { finalized.countDown() }
            assertTrue(finalized.await(5, TimeUnit.SECONDS))
            awaitClosed(owner)
        }
        assertEquals(1, cleared.get())
    }

    @Test fun failedWorkerStartRemovesAnAlreadyQueuedInputBeforeRetry() {
        val owner = OwnedExecutor()
        val backend = idleBackend(owner, 0)
        val originalFactory = backend.threadFactory
        val cleared = AtomicInteger()
        val completed = CountDownLatch(1)
        val finalized = CountDownLatch(1)
        val problem = SecurityException("Synthetic public thread-start refusal")
        try {
            backend.corePoolSize = 0 // Exercise execute's enqueue-then-start branch.
            backend.threadFactory = ThreadFactory { throw problem }
            assertSame(problem, assertFailsWith<SecurityException> {
                owner.submit({ cleared.incrementAndGet() }) { error("Failed submission ran after retry") }
            })
            assertEquals(1, cleared.get())
            assertTrue(backend.queue.isEmpty())
            backend.threadFactory = originalFactory
            assertTrue(owner.submit { completed.countDown() })
            assertTrue(completed.await(5, TimeUnit.SECONDS))
        } finally {
            backend.threadFactory = originalFactory
            owner.close { finalized.countDown() }
            assertTrue(finalized.await(5, TimeUnit.SECONDS))
            awaitClosed(owner)
        }
        assertEquals(1, cleared.get())
    }

    @Test fun cleanupFailureCannotHideTheOriginalWorkerStartError() {
        for (coreSize in listOf(0, 1)) {
            val owner = OwnedExecutor()
            val backend = idleBackend(owner, coreSize)
            val originalFactory = backend.threadFactory
            val first = OutOfMemoryError("Public worker-start failure")
            val second = IllegalStateException("Public cleanup failure")
            val clears = AtomicInteger()
            val ran = CountDownLatch(1)
            val words = charArrayOf('a', 'b', 'c')
            try {
                backend.threadFactory = ThreadFactory { throw first }
                assertSame(first, assertFailsWith<Throwable> {
                    owner.submit({ words.fill('\u0000'); clears.incrementAndGet(); throw second }) {
                        error("Failed input ran")
                    }
                })
                assertTrue(words.all { it == '\u0000' })
                assertEquals(1, clears.get())
                assertTrue(backend.queue.isEmpty())
                backend.threadFactory = originalFactory
                assertTrue(owner.submit { ran.countDown() })
                assertTrue(ran.await(5, TimeUnit.SECONDS))
            } finally {
                backend.threadFactory = originalFactory
                owner.close()
                awaitClosed(owner)
                words.fill('\u0000')
            }
            assertEquals(1, clears.get())
        }
        assertBothAdmissionsAvailable()
    }

    @Test fun closedExecutorRejectsAndClearsInputsOnce() {
        val executor = OwnedExecutor()
        val cleared = AtomicInteger()
        val finalized = CountDownLatch(1)
        executor.close { finalized.countDown() }
        assertFalse(executor.submit({ cleared.incrementAndGet() }) { error("Closed work ran") })
        assertTrue(finalized.await(5, TimeUnit.SECONDS))
        assertEquals(1, cleared.get())
        executor.close { error("Session finalized twice") }
        awaitClosed(executor)
    }

    @Test fun closingAnIdlePoolNeedsNoReplacementThreadAndAlwaysClearsItsSession() {
        for (coreSize in listOf(0, 1)) {
            for (mode in 0..2) verifyCloseWithoutWorker(coreSize, mode)
        }
    }

    @Test fun aFailingFinalizerStillTerminatesAndReleasesAdmission() {
        val owner = OwnedExecutor()
        val backend = idleBackend(owner, 1)
        val clears = AtomicInteger()
        val problem = OutOfMemoryError("Synthetic public finalizer failure")
        try {
            assertSame(problem, assertFailsWith<OutOfMemoryError> {
                owner.close { clears.incrementAndGet(); throw problem }
            })
            assertTrue(owner.isClosed)
            assertEquals(1, clears.get())
            assertTrue(backend.awaitTermination(5, TimeUnit.SECONDS))
            owner.close { error("Failed finalizer retried") }
        } finally { owner.close(); awaitClosed(owner) }
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
            assertEquals(0, attempts.get(), "Closing must not request a replacement thread")
            assertEquals(1, clears.get())
            assertTrue(words.all { it == '\u0000' })
            assertTrue(backend.queue.isEmpty())
            assertTrue(backend.awaitTermination(5, TimeUnit.SECONDS))
            owner.close { error("Session finalized twice") }
            assertFalse(owner.submit { error("Closed task ran") })
        } finally {
            // Keep even a failing regression/mutant fixture locally bounded.
            // Any retained task here is only this test's public cleanup.
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
        try {
            for (owner in listOf(first, second)) assertTrue(owner.submit {
                entered.countDown()
                check(release.await(10, TimeUnit.SECONDS))
            })
            assertTrue(entered.await(5, TimeUnit.SECONDS))
        } finally {
            release.countDown()
            first.close()
            second.close()
            awaitClosed(first)
            awaitClosed(second)
        }
    }

    @Test fun queueIsBoundedAndCancellationClearsAfterActiveWork() {
        val executor = OwnedExecutor()
        val entered = CountDownLatch(1)
        val release = CountDownLatch(1)
        val finalized = CountDownLatch(1)
        val cleaned = AtomicInteger()
        val ran = AtomicInteger()
        val finalCount = AtomicInteger(-1)
        try {
            assertTrue(executor.submit({ cleaned.incrementAndGet() }) {
                entered.countDown()
                check(release.await(5, TimeUnit.SECONDS))
                ran.incrementAndGet()
            })
            assertTrue(entered.await(5, TimeUnit.SECONDS))
            repeat(4) {
                assertTrue(executor.submit({ cleaned.incrementAndGet() }) { ran.incrementAndGet() })
            }
            assertFalse(executor.submit({ cleaned.incrementAndGet() }) { ran.incrementAndGet() })
            assertEquals(1, cleaned.get()) // Rejected work was cleared immediately.
            executor.close {
                finalCount.set(cleaned.get())
                finalized.countDown()
            }
            assertEquals(5, cleaned.get()) // Four queued inputs were discarded.
            release.countDown()
            assertTrue(finalized.await(5, TimeUnit.SECONDS))
            assertEquals(1, ran.get())
            assertEquals(6, cleaned.get())
            assertEquals(6, finalCount.get())
        } finally {
            release.countDown()
            executor.close()
            awaitClosed(executor)
        }
    }

    @Test fun failingQueuedCleanupStillDrainsAndShutsDownTheOwner() {
        for (fatal in listOf(false, true)) verifyFailingQueuedCleanup(fatal)
    }

    private fun verifyFailingQueuedCleanup(fatal: Boolean) {
        val owner = OwnedExecutor()
        val entered = CountDownLatch(1)
        val release = CountDownLatch(1)
        val clears = AtomicInteger()
        val finalCount = AtomicInteger(-1)
        val words = Array(4) { charArrayOf('a', 'b', 'c') }
        val first = if (fatal) OutOfMemoryError("Public queued cleanup failure")
            else IllegalStateException("Public queued cleanup failure")
        val second = IllegalStateException("Public secondary cleanup failure")
        var pool: ThreadPoolExecutor? = null
        try {
            assertTrue(owner.submit({ clears.incrementAndGet() }) {
                entered.countDown()
                check(release.await(15, TimeUnit.SECONDS))
            })
            assertTrue(entered.await(5, TimeUnit.SECONDS))
            pool = OwnedExecutor::class.java.getDeclaredField("executor").apply {
                isAccessible = true
            }.get(owner) as ThreadPoolExecutor
            for (index in words.indices) assertTrue(owner.submit({
                words[index].fill('\u0000')
                clears.incrementAndGet()
                if (index == 0) throw first
                if (index == 1) throw second
            }) { error("Cancelled action ran") })
            assertSame(first, assertFailsWith<Throwable> {
                owner.close { finalCount.set(clears.get()) }
            })
            assertTrue(owner.isClosed)
            assertEquals(4, clears.get(), "A failed cleanup stranded later queued inputs")
            assertTrue(words.all { input -> input.all { it == '\u0000' } })
            assertTrue(pool.queue.isEmpty())
            assertTrue(pool.isShutdown)
            assertEquals(-1, finalCount.get(), "Session cleanup raced active work")
            owner.close { error("Closed owner finalized twice") }
            release.countDown()
            assertTrue(pool.awaitTermination(5, TimeUnit.SECONDS))
            assertEquals(5, clears.get())
            assertEquals(5, finalCount.get())
        } finally {
            release.countDown()
            // Retain bounded cleanup even against the unfixed implementation.
            pool?.let { backend ->
                while (true) {
                    val task = backend.queue.poll() ?: break
                    try { task.run() }
                    catch (problem: Throwable) { assertTrue(problem === first || problem === second) }
                }
                backend.shutdown()
                assertTrue(backend.awaitTermination(5, TimeUnit.SECONDS))
            }
            owner.close()
            words.forEach { it.fill('\u0000') }
        }
        assertBothAdmissionsAvailable()
    }
}
