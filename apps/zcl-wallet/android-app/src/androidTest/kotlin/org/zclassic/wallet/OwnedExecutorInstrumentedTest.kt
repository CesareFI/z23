// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import androidx.test.ext.junit.runners.AndroidJUnit4
import java.util.concurrent.CountDownLatch
import java.util.concurrent.ThreadFactory
import java.util.concurrent.ThreadPoolExecutor
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicInteger
import org.junit.Assert.assertEquals
import org.junit.Assert.assertSame
import org.junit.Assert.assertThrows
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

/** Controlled thread-factory faults only; no memory exhaustion or wallet state. */
@RunWith(AndroidJUnit4::class)
class OwnedExecutorInstrumentedTest {
    @Test fun androidWorkerStartFailureClearsInputAndAllowsAnIndependentRetry() {
        for (coreSize in listOf(0, 1)) verifyFailedHandoff(coreSize)
    }

    private fun verifyFailedHandoff(coreSize: Int) {
        val owner = OwnedExecutor()
        val field = OwnedExecutor::class.java.getDeclaredField("executor").apply { isAccessible = true }
        val backend = field.get(owner) as ThreadPoolExecutor
        val originalFactory = backend.threadFactory
        val clears = AtomicInteger()
        val ran = CountDownLatch(1)
        val finalized = CountDownLatch(1)
        val words = charArrayOf('a', 'b', 'c')
        val problem = OutOfMemoryError("Synthetic public thread-allocation failure")
        try {
            backend.corePoolSize = coreSize
            backend.threadFactory = ThreadFactory { throw problem }
            val caught = assertThrows(OutOfMemoryError::class.java) {
                owner.submit({ words.fill('\u0000'); clears.incrementAndGet() }) {
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
        }
        assertEquals(1, clears.get())
    }
}
