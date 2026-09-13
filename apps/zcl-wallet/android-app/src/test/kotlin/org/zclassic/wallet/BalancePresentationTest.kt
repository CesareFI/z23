// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import java.util.concurrent.ConcurrentLinkedQueue
import java.util.concurrent.CountDownLatch
import java.util.concurrent.Executor
import java.util.concurrent.RejectedExecutionException
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicInteger
import java.util.concurrent.atomic.AtomicLong
import java.util.concurrent.atomic.AtomicReference
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertFalse
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue
import org.zclassic.wallet.core.CoreStatus
import org.zclassic.wallet.core.Network
import org.zclassic.wallet.core.ReadOnlySync
import org.zclassic.wallet.core.ReadOnlySyncFailure
import org.zclassic.wallet.core.TransparentAddress

class BalancePresentationTest {
    private class DelayedUi : Executor {
        val callbacks = ConcurrentLinkedQueue<Runnable>()
        override fun execute(command: Runnable) { callbacks.add(command) }
        fun runNext() { checkNotNull(callbacks.poll()).run() }
    }
    private val now = AtomicLong(0)
    private val samples = AtomicInteger(0)
    private class DelayedWakeup : BalanceWakeup {
        var delay: Long? = null
        var callback: Runnable? = null
        var rejecting = false
        override fun replace(delayMillis: Long, callback: Runnable): Boolean {
            cancel()
            if (rejecting) return false
            check(delayMillis > 0)
            delay = delayMillis
            this.callback = callback
            return true
        }
        override fun cancel() { callback = null; delay = null }
        fun fire() {
            val next = checkNotNull(callback)
            cancel()
            next.run()
        }
    }
    private fun presentation(sync: ReadOnlySync, ui: Executor,
                             receive: (ReadOnlySync.Snapshot) -> Unit,
                             unavailable: (CoreStatus) -> Unit) =
        BalancePresentation(sync, ui, DelayedWakeup(), receive, unavailable)
    private fun owner(clock: () -> Long = { samples.incrementAndGet(); now.get() }): ReadOnlySync =
        ReadOnlySync(TransparentAddress.fromPublicKeyHash(ByteArray(20), Network.MAINNET),
                     ByteArray(32), clock)
    private fun complete(sync: ReadOnlySync) {
        val attempt = sync.begin()
        for (step in 1..6) {
            attempt.request()
            val frame = checkNotNull(javaClass.getResourceAsStream("/sync/mainnet-$step.json"))
                .use { it.readBytes() }
            assertEquals(CoreStatus.OK, attempt.reply(frame))
        }
    }

    @Test fun delayedRedrawSamplesCurrentFreshnessAndCoalescesWorkerSignals() {
        val ui = DelayedUi()
        val sync = owner()
        val delivered = mutableListOf<ReadOnlySync.Snapshot>()
        presentation(sync, ui, {
            assertFalse(Thread.holdsLock(sync))
            delivered.add(it)
        }, { error("Unexpected unavailable state: $it") }).use { presentation ->
            complete(sync)
            val before = samples.get()
            repeat(128) { assertTrue(presentation.requestUpdate()) }
            assertEquals(1, ui.callbacks.size)
            assertTrue(delivered.isEmpty())
            assertEquals(before, samples.get())
            now.set(60_000)
            ui.runNext()
            assertEquals(ReadOnlySync.Freshness.STALE, delivered.single().freshness)
            assertEquals(993L, assertNotNull(delivered.single().report).total.value)
            assertEquals(before + 1, samples.get())
        }
    }

    @Test fun closeCancelsQueuedReadsAndReplacementStartsUnavailable() {
        val ui = DelayedUi()
        val first = owner()
        val old = presentation(first, ui, { error("Closed session rendered") },
                                      { error("Closed session notified") })
        try {
            complete(first)
            assertTrue(old.requestUpdate())
            old.close()
            val before = samples.get()
            owner().let { replacement ->
                presentation(replacement, ui, {
                    assertNull(it.report)
                    assertEquals(ReadOnlySync.Freshness.UNAVAILABLE, it.freshness)
                }, { error("Replacement failed") }).use { current ->
                    assertTrue(current.requestUpdate())
                    ui.runNext()
                    assertEquals(before, samples.get())
                    ui.runNext()
                    assertEquals(before + 1, samples.get())
                }
            }
            assertFalse(old.requestUpdate())
        } finally { old.close() }
    }

    @Test fun rejectedQueueReleasesPendingSignalForExplicitRetry() {
        val ui = DelayedUi()
        var rejecting = true
        var deliveries = 0
        presentation(owner(), { command ->
            if (rejecting) throw RejectedExecutionException("Public fixture rejection")
            ui.execute(command)
        }, { deliveries++ }, { error("Unexpected failure") }).use { presentation ->
            assertFalse(presentation.requestUpdate())
            assertTrue(ui.callbacks.isEmpty())
            rejecting = false
            assertTrue(presentation.requestUpdate())
            ui.runNext()
            assertEquals(1, deliveries)
        }
    }

    @Test fun queuedRedrawObservesOfflineRefreshFailure() {
        val ui = DelayedUi()
        val sync = owner()
        var delivered: ReadOnlySync.Snapshot? = null
        presentation(sync, ui, { delivered = it }, { error("Unexpected failure") }).use {
            complete(sync)
            assertTrue(it.requestUpdate())
            assertEquals(CoreStatus.IO_FAILURE, sync.begin().fail(CoreStatus.IO_FAILURE))
            ui.runNext()
            val current = assertNotNull(delivered)
            assertEquals(ReadOnlySync.Freshness.STALE, current.freshness)
            assertEquals(CoreStatus.IO_FAILURE, current.lastFault)
            assertFalse(current.refreshing)
            assertEquals(993L, assertNotNull(current.report).total.value)
        }
    }

    @Test fun nativeOwnerFailureClearsPresentationInsteadOfDeliveringBalance() {
        val ui = DelayedUi()
        val sync = owner()
        var fault: CoreStatus? = null
        presentation(sync, ui, { error("Failed owner rendered") }, { fault = it }).use {
            assertTrue(it.requestUpdate())
            sync.close() // Simulate an independently invalidated native lifetime.
            ui.runNext()
            assertEquals(CoreStatus.CANCELLED, fault)
            assertFalse(it.requestUpdate())
        }
    }

    @Test fun clockFailureClosesOwnerAndReportsOnlyStableStatus() {
        val ui = DelayedUi()
        val sync = owner { throw IllegalStateException("Public fixture clock failure") }
        var fault: CoreStatus? = null
        presentation(sync, ui, { error("Clock failure rendered") }, { fault = it }).use {
            assertTrue(it.requestUpdate())
            ui.runNext()
            assertEquals(CoreStatus.IO_UNCERTAIN, fault)
            assertFalse(it.requestUpdate())
            assertEquals(CoreStatus.CANCELLED,
                assertFailsWith<ReadOnlySyncFailure> { sync.snapshot() }.status)
        }
    }

    @Test fun receiverFailureClosesOwnerAndPropagates() {
        val ui = DelayedUi()
        val sync = owner()
        presentation(sync, ui, { error("Public fixture receiver failure") },
                            { error("Receiver failure is not a sync fault") }).use {
            assertTrue(it.requestUpdate())
            assertFailsWith<IllegalStateException> { ui.runNext() }
            assertFalse(it.requestUpdate())
            assertEquals(CoreStatus.CANCELLED,
                assertFailsWith<ReadOnlySyncFailure> { sync.snapshot() }.status)
        }
    }

    @Test fun concurrentWorkersCoalesceWithoutRunningUiCallbacks() {
        val ui = DelayedUi()
        val start = CountDownLatch(1)
        val problem = AtomicReference<Throwable>()
        var delivered = 0
        presentation(owner(), ui, { delivered++ }, { error("Unexpected failure") }).use { owner ->
            val workers = List(2) {
                Thread {
                    try {
                        check(start.await(5, TimeUnit.SECONDS))
                        repeat(128) { check(owner.requestUpdate()) }
                    } catch (failure: Throwable) { problem.set(failure) }
                }
            }
            try {
                workers.forEach { it.start() }
                start.countDown()
                workers.forEach { it.join(5000) }
                assertTrue(workers.none { it.isAlive })
                assertNull(problem.get())
                assertEquals(1, ui.callbacks.size)
                assertEquals(0, samples.get())
                ui.runNext()
                assertEquals(1, delivered)
            } finally {
                start.countDown()
                workers.forEach { it.join(5000) }
            }
        }
    }

    @Test fun closingDuringSubmissionCannotRenderLateCallback() {
        val entered = CountDownLatch(1)
        val release = CountDownLatch(1)
        val callback = AtomicReference<Runnable>()
        val problem = AtomicReference<Throwable>()
        val presentation = presentation(owner(), { command ->
            entered.countDown()
            check(release.await(5, TimeUnit.SECONDS))
            callback.set(command)
        }, { error("Closed presentation rendered") }, { error("Closed presentation notified") })
        val worker = Thread {
            try { presentation.requestUpdate() }
            catch (failure: Throwable) { problem.set(failure) }
        }
        try {
            worker.start()
            assertTrue(entered.await(5, TimeUnit.SECONDS))
            presentation.close()
        } finally {
            release.countDown()
            worker.join(5000)
            presentation.close()
        }
        assertFalse(worker.isAlive)
        assertNull(problem.get())
        checkNotNull(callback.get()).run()
        assertEquals(0, samples.get())
    }

    @Test fun earlyAndLateWakeupsAlwaysRecheckCAndStopWhenStale() {
        val ui = DelayedUi()
        val wakeup = DelayedWakeup()
        val sync = owner()
        val delivered = mutableListOf<ReadOnlySync.Snapshot>()
        BalancePresentation(sync, ui, wakeup, { delivered.add(it) }, { error("Unexpected fault") }).use {
            complete(sync)
            it.requestUpdate()
            ui.runNext()
            assertEquals(60000L, wakeup.delay)
            now.set(59999)
            wakeup.fire() // A scheduler firing early cannot decide that the report expired.
            ui.runNext()
            assertEquals(ReadOnlySync.Freshness.UNVERIFIED, delivered.last().freshness)
            assertEquals(1L, wakeup.delay)
            now.set(60007)
            wakeup.fire()
            assertEquals(2, delivered.size) // Timer carries no snapshot; only the UI queue reads it.
            ui.runNext()
            assertEquals(ReadOnlySync.Freshness.STALE, delivered.last().freshness)
            assertNull(wakeup.callback)
            assertNull(wakeup.delay)
        }
    }

    @Test fun wakeupExpiresAnAttemptWithoutAnotherProducerEvent() {
        val ui = DelayedUi()
        val wakeup = DelayedWakeup()
        val sync = owner()
        var delivered: ReadOnlySync.Snapshot? = null
        BalancePresentation(sync, ui, wakeup, { delivered = it }, { error("Unexpected fault") }).use {
            sync.begin(10)
            it.requestUpdate()
            ui.runNext()
            assertEquals(10L, wakeup.delay)
            assertTrue(checkNotNull(delivered).refreshing)
            now.set(10)
            wakeup.fire()
            ui.runNext()
            val expired = checkNotNull(delivered)
            assertFalse(expired.refreshing)
            assertNull(expired.report)
            assertEquals(CoreStatus.TIMED_OUT, expired.lastFault)
            assertNull(wakeup.callback)
        }
    }

    @Test fun rejectedWakeupDoesNotPublishANewUnverifiedDisplay() {
        val ui = DelayedUi()
        val wakeup = DelayedWakeup().apply { rejecting = true }
        val sync = owner()
        var fault: CoreStatus? = null
        BalancePresentation(sync, ui, wakeup, { error("Unscheduled balance rendered") }, { fault = it }).use {
            complete(sync)
            it.requestUpdate()
            ui.runNext()
            assertEquals(CoreStatus.RESOURCE_EXHAUSTED, fault)
            assertFalse(it.requestUpdate())
            assertNull(wakeup.callback)
            assertEquals(CoreStatus.CANCELLED,
                assertFailsWith<ReadOnlySyncFailure> { sync.snapshot() }.status)
        }
    }

    @Test fun closeCancelsWakeupAndRejectsItsAlreadyCapturedCallback() {
        val ui = DelayedUi()
        val wakeup = DelayedWakeup()
        val sync = owner()
        var deliveries = 0
        BalancePresentation(sync, ui, wakeup, { deliveries++ }, { error("Unexpected fault") }).use {
            complete(sync)
            it.requestUpdate()
            ui.runNext()
            val late = checkNotNull(wakeup.callback)
            val before = samples.get()
            it.close()
            assertNull(wakeup.callback)
            now.set(60000)
            late.run()
            assertTrue(ui.callbacks.isEmpty())
            assertEquals(before, samples.get())
            assertEquals(1, deliveries)
        }
    }

    @Test fun receiverFailureAlsoCancelsAnArmedWakeup() {
        val ui = DelayedUi()
        val wakeup = DelayedWakeup()
        val sync = owner()
        BalancePresentation(sync, ui, wakeup, { error("Public fixture receiver failure") },
                            { error("Receiver failure is not a sync fault") }).use {
            complete(sync)
            it.requestUpdate()
            assertFailsWith<IllegalStateException> { ui.runNext() }
            assertNull(wakeup.callback)
            assertFalse(it.requestUpdate())
        }
    }

    @Test fun timerCoalescesWithPendingProducerAndClosesOnQueueRejection() {
        val ui = DelayedUi()
        val wakeup = DelayedWakeup()
        val sync = owner()
        var reject = false
        var fault: CoreStatus? = null
        BalancePresentation(sync, { command ->
            if (reject) throw RejectedExecutionException("Public fixture queue rejection")
            ui.execute(command)
        }, wakeup, {}, { fault = it }).use {
            complete(sync)
            it.requestUpdate()
            ui.runNext()
            it.requestUpdate()
            wakeup.fire()
            assertEquals(1, ui.callbacks.size)
            ui.runNext()
            reject = true
            wakeup.fire()
            assertEquals(CoreStatus.RESOURCE_EXHAUSTED, fault)
            assertNull(wakeup.callback)
            assertFalse(it.requestUpdate())
        }
    }
}
