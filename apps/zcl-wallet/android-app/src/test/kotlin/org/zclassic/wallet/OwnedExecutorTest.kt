// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import java.util.concurrent.ThreadFactory
import java.util.concurrent.ThreadPoolExecutor
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
}
