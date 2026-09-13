// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import java.util.concurrent.Executor
import java.util.concurrent.CountDownLatch
import java.util.concurrent.RejectedExecutionException
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicReference
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertSame
import kotlin.test.assertTrue

class RecoveryPhraseDeliveryTest {
    private class DelayedUi : Executor {
        val callbacks = ArrayDeque<Runnable>()
        override fun execute(command: Runnable) { callbacks.addLast(command) }
        fun runNext() { callbacks.removeFirst().run() }
    }

    @Test fun closingClearsPhraseBeforeDelayedCallbackRuns() {
        val ui = DelayedUi()
        val owner = RecoveryPhraseDelivery(ui)
        val words = charArrayOf('a', 'b', 'c') // Public markers only.
        owner.post(words) { error("Closed owner delivered recovery words") }
        assertContentEquals(charArrayOf('a', 'b', 'c'), words)
        owner.close()
        assertTrue(words.all { it == '\u0000' })
        assertEquals(1, ui.callbacks.size)
        ui.runNext()
        owner.close()
    }

    @Test fun closedOwnerClearsWorkerCompletionWithoutQueueing() {
        val ui = DelayedUi()
        val owner = RecoveryPhraseDelivery(ui)
        owner.close()
        val words = charArrayOf('a')
        owner.post(words) { error("Late phrase was delivered") }
        assertTrue(words.all { it == '\u0000' })
        assertTrue(ui.callbacks.isEmpty())
    }

    @Test fun rejectedExecutorClearsPhraseAndReleasesPendingSlot() {
        val ui = DelayedUi()
        var rejecting = true
        val owner = RecoveryPhraseDelivery { command ->
            if (rejecting) throw RejectedExecutionException("Public fixture rejection")
            ui.execute(command)
        }
        val rejected = charArrayOf('a')
        assertFailsWith<RejectedExecutionException> { owner.post(rejected) {} }
        assertTrue(rejected.all { it == '\u0000' })
        rejecting = false
        val next = charArrayOf('b')
        owner.post(next) { error("Closing must cancel retry delivery") }
        owner.close()
        assertTrue(next.all { it == '\u0000' })
        ui.runNext()
    }

    @Test fun pendingSlotIsBoundedAndRejectedInputIsCleared() {
        val ui = DelayedUi()
        val owner = RecoveryPhraseDelivery(ui)
        val first = charArrayOf('a')
        val second = charArrayOf('b')
        owner.post(first) { assertSame(first, it) }
        assertFailsWith<IllegalStateException> { owner.post(second) {} }
        assertTrue(second.all { it == '\u0000' })
        assertContentEquals(charArrayOf('a'), first)
        ui.runNext()
        owner.close()
        // A successfully delivered array belongs to the receiver, not the queue.
        assertContentEquals(charArrayOf('a'), first)
        first.fill('\u0000')
    }

    @Test fun failedReceiverClearsInputAndCannotDeliverTwice() {
        val ui = DelayedUi()
        val owner = RecoveryPhraseDelivery(ui)
        val words = charArrayOf('a')
        owner.post(words) { throw IllegalStateException("Public fixture receiver failure") }
        val callback = ui.callbacks.removeFirst()
        assertFailsWith<IllegalStateException> { callback.run() }
        assertTrue(words.all { it == '\u0000' })
        callback.run()
        owner.close()
    }

    @Test fun excessiveInputIsClearedBeforeQueueing() {
        val ui = DelayedUi()
        val owner = RecoveryPhraseDelivery(ui)
        val words = CharArray(216) { 'a' }
        assertFailsWith<IllegalArgumentException> { owner.post(words) {} }
        assertTrue(words.all { it == '\u0000' })
        assertTrue(ui.callbacks.isEmpty())
        owner.close()
    }

    @Test fun closingDuringWorkerSubmissionClearsWithoutWaitingForExecutor() {
        val entered = CountDownLatch(1)
        val release = CountDownLatch(1)
        val queued = AtomicReference<Runnable>()
        val failure = AtomicReference<Throwable>()
        val owner = RecoveryPhraseDelivery { command ->
            entered.countDown()
            check(release.await(5, TimeUnit.SECONDS))
            queued.set(command)
        }
        val words = charArrayOf('a')
        val worker = Thread {
            try { owner.post(words) { error("Closed owner delivered late submission") } }
            catch (problem: Throwable) { failure.set(problem) }
        }
        try {
            worker.start()
            assertTrue(entered.await(5, TimeUnit.SECONDS))
            owner.close()
            assertTrue(words.all { it == '\u0000' })
        } finally {
            release.countDown()
            worker.join(5000)
            owner.close()
        }
        assertTrue(!worker.isAlive)
        assertEquals(null, failure.get())
        checkNotNull(queued.get()).run()
    }
}
