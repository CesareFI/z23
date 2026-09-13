// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicInteger
import java.util.concurrent.atomic.AtomicLong
import java.util.concurrent.atomic.AtomicReference
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.zclassic.wallet.core.CoreStatus
import org.zclassic.wallet.core.Network
import org.zclassic.wallet.core.ReadOnlySync
import org.zclassic.wallet.core.TransparentAddress

/** Public local fixtures with real Android main-queue delivery. No wallet or endpoint. */
@RunWith(AndroidJUnit4::class)
class BalancePresentationInstrumentedTest {
    private val instrumentation = InstrumentationRegistry.getInstrumentation()
    private fun owner(now: AtomicLong) = ReadOnlySync(
        TransparentAddress.fromPublicKeyHash(ByteArray(20), Network.MAINNET), ByteArray(32), now::get)
    private fun complete(sync: ReadOnlySync) {
        val attempt = sync.begin()
        for (step in 1..6) {
            attempt.request()
            val frame = instrumentation.context.assets.open("sync/mainnet-$step.json").use { it.readBytes() }
            assertEquals(CoreStatus.OK, attempt.reply(frame))
        }
    }
    private fun postFromWorker(presentation: BalancePresentation) {
        val problem = AtomicReference<Throwable>()
        val worker = Thread {
            try { check(presentation.requestUpdate()) }
            catch (failure: Throwable) { problem.set(failure) }
        }
        worker.start()
        worker.join(5000)
        check(!worker.isAlive) { "Public fixture worker did not finish" }
        problem.get()?.let { throw it }
    }

    @Test fun mainQueueDelayRechecksAgeBeforeDelivering() {
        val now = AtomicLong(0)
        val sync = owner(now)
        var presentation: BalancePresentation? = null
        val delivered = AtomicReference<ReadOnlySync.Snapshot>()
        val fault = AtomicReference<CoreStatus>()
        val ready = CountDownLatch(1)
        try {
            complete(sync)
            instrumentation.runOnMainSync {
                val current = BalancePresentation(sync, instrumentation.targetContext.mainExecutor,
                    { delivered.set(it); ready.countDown() }, { fault.set(it); ready.countDown() })
                presentation = current
                postFromWorker(current) // Main callback cannot run until this block ends.
                now.set(60_000)
            }
            assertTrue(ready.await(5, TimeUnit.SECONDS))
            assertNull(fault.get())
            val current = checkNotNull(delivered.get())
            assertEquals(ReadOnlySync.Freshness.STALE, current.freshness)
            assertEquals(993L, checkNotNull(current.report).total.value)
        } finally {
            instrumentation.runOnMainSync { presentation?.close() }
            sync.close()
        }
    }

    @Test fun replacementDropsOldQueuedCallbackAndStartsWithoutBalance() {
        val now = AtomicLong(0)
        val first = owner(now)
        var old: BalancePresentation? = null
        var replacement: BalancePresentation? = null
        val oldCallbacks = AtomicInteger(0)
        val delivered = AtomicReference<ReadOnlySync.Snapshot>()
        val fault = AtomicReference<CoreStatus>()
        val ready = CountDownLatch(1)
        try {
            complete(first)
            instrumentation.runOnMainSync {
                val current = BalancePresentation(first, instrumentation.targetContext.mainExecutor,
                    { oldCallbacks.incrementAndGet() }, { oldCallbacks.incrementAndGet() })
                old = current
                postFromWorker(current)
                current.close()
                val next = BalancePresentation(owner(now), instrumentation.targetContext.mainExecutor,
                    { delivered.set(it); ready.countDown() }, { fault.set(it); ready.countDown() })
                replacement = next
                postFromWorker(next)
            }
            assertTrue(ready.await(5, TimeUnit.SECONDS))
            instrumentation.waitForIdleSync()
            assertEquals(0, oldCallbacks.get())
            assertNull(fault.get())
            val current = checkNotNull(delivered.get())
            assertEquals(ReadOnlySync.Freshness.UNAVAILABLE, current.freshness)
            assertNull(current.report)
            assertFalse(current.refreshing)
        } finally {
            instrumentation.runOnMainSync { old?.close(); replacement?.close() }
            first.close()
        }
    }
}
