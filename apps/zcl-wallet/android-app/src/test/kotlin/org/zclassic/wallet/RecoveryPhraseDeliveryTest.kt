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

    /** Observe the actual queued holder, without GC timing or a production
     * inspection API. The holder must retire its strong references to both
     * transferred inputs even if an executor keeps its Runnable indefinitely. */
    private fun pendingDelivery(owner: RecoveryPhraseDelivery): Any =
        checkNotNull(RecoveryPhraseDelivery::class.java.getDeclaredField("pending").apply {
            isAccessible = true
        }.get(owner))

    private fun assertRetired(holder: Any, words: CharArray, receiver: (CharArray) -> Unit) {
        for (field in holder.javaClass.declaredFields) {
            if (java.lang.reflect.Modifier.isStatic(field.modifiers)) continue
            field.isAccessible = true
            val value = field.get(holder)
            assertTrue(value !== words, "Queued delivery retains the character array")
            assertTrue(value !== receiver, "Queued delivery retains the receiver")
        }
    }

    @Test fun cancellationRetiresQueuedInputsBeforeTheExecutorRuns() {
        val ui = DelayedUi()
        val owner = RecoveryPhraseDelivery(ui)
        val words = charArrayOf('a', 'b', 'c')
        val receiver: (CharArray) -> Unit = { error("Cancelled receiver ran") }
        owner.post(words, receiver)
        val holder = pendingDelivery(owner)
        try {
            owner.close()
            assertTrue(words.all { it == '\u0000' })
            assertEquals(1, ui.callbacks.size)
            assertRetired(holder, words, receiver)
            ui.runNext()
        } finally { owner.close(); words.fill('\u0000') }
    }

    @Test fun claimingRetiresQueuedInputsBeforeCallingTheReceiver() {
        for (fail in listOf(false, true)) {
            val ui = DelayedUi()
            val owner = RecoveryPhraseDelivery(ui)
            val words = charArrayOf('a', 'b', 'c')
            lateinit var holder: Any
            lateinit var receiver: (CharArray) -> Unit
            var calls = 0
            val failure = IllegalStateException("Public receiver failure")
            receiver = {
                ++calls
                assertSame(words, it)
                assertRetired(holder, words, receiver)
                owner.close() // Reentrancy cannot erase the receiver's input.
                assertContentEquals(charArrayOf('a', 'b', 'c'), it)
                if (fail) throw failure
            }
            try {
                owner.post(words, receiver)
                holder = pendingDelivery(owner)
                val callback = ui.callbacks.removeFirst()
                if (fail) assertSame(failure, assertFailsWith<IllegalStateException> { callback.run() })
                else callback.run()
                assertRetired(holder, words, receiver)
                if (fail) assertTrue(words.all { it == '\u0000' })
                else assertContentEquals(charArrayOf('a', 'b', 'c'), words)
                callback.run()
                assertEquals(1, calls)
            } finally { owner.close(); words.fill('\u0000') }
        }
    }

    @Test fun enqueueThenThrowRetiresCancelledInputsAndAllowsRetry() {
        for (fatal in listOf(false, true)) {
            val ui = DelayedUi()
            var rejecting = true
            val failure = if (fatal) OutOfMemoryError("Public queue failure")
                else RejectedExecutionException("Public queue refusal")
            lateinit var owner: RecoveryPhraseDelivery
            lateinit var holder: Any
            owner = RecoveryPhraseDelivery { command ->
                ui.execute(command)
                if (rejecting) {
                    holder = pendingDelivery(owner)
                    throw failure
                }
            }
            val words = charArrayOf('a')
            val receiver: (CharArray) -> Unit = { error("Rejected receiver ran") }
            try {
                assertSame(failure, assertFailsWith<Throwable> { owner.post(words, receiver) })
                assertTrue(words.all { it == '\u0000' })
                assertRetired(holder, words, receiver)
                rejecting = false
                val next = charArrayOf('b')
                owner.post(next) { assertContentEquals(charArrayOf('b'), it) }
                ui.runNext() // A stale queued task cannot claim the replacement.
                assertContentEquals(charArrayOf('b'), next)
                ui.runNext()
                next.fill('\u0000')
            } finally { owner.close(); words.fill('\u0000') }
        }
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
