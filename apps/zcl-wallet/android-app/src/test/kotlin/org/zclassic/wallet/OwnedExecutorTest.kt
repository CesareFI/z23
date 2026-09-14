// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import java.util.concurrent.ThreadFactory
import java.util.concurrent.ThreadPoolExecutor
import java.util.concurrent.atomic.AtomicInteger
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertFailsWith
import kotlin.test.assertSame
import kotlin.test.assertTrue

class OwnedExecutorTest {
    @Test fun threadCreationFailureClearsTransferredInputAndPreservesFailure() {
        val owner = OwnedExecutor()
        val field = OwnedExecutor::class.java.getDeclaredField("executor").apply { isAccessible = true }
        val backend = field.get(owner) as ThreadPoolExecutor
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
        }
        assertEquals(1, cleared.get())
    }

    @Test fun failedWorkerStartRemovesAnAlreadyQueuedInputBeforeRetry() {
        val owner = OwnedExecutor()
        val field = OwnedExecutor::class.java.getDeclaredField("executor").apply { isAccessible = true }
        val backend = field.get(owner) as ThreadPoolExecutor
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
        }
    }
}
