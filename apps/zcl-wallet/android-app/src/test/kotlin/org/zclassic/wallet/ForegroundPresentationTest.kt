// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import java.util.concurrent.ConcurrentLinkedQueue
import java.util.concurrent.Executor
import java.util.concurrent.atomic.AtomicReference
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertFalse
import kotlin.test.assertIs
import kotlin.test.assertNull
import kotlin.test.assertSame
import kotlin.test.assertTrue
import org.zclassic.wallet.core.CoreStatus

internal class PresentationTestQueue : Executor {
    val callbacks = ConcurrentLinkedQueue<Runnable>()
    override fun execute(command: Runnable) { callbacks.add(command) }
    fun runNext() { checkNotNull(callbacks.poll()).run() }
}

internal class PresentationTestWakeup : BalanceWakeup {
    var delay: Long? = null
    var callback: Runnable? = null
    var rejecting = false
    var cancelProblem: Throwable? = null
    override fun replace(delayMillis: Long, callback: Runnable): Boolean {
        cancel()
        if (rejecting) return false
        check(delayMillis > 0)
        delay = delayMillis
        this.callback = callback
        return true
    }
    override fun cancel() {
        callback = null
        delay = null
        cancelProblem?.let { throw it }
    }
    fun fire() {
        val next = checkNotNull(callback)
        cancel()
        next.run()
    }
}

class ForegroundPresentationTest {
    @Test fun closeFailureStillClearsDisplayAndPreservesBothCleanupProblems() {
        val ui = PresentationTestQueue()
        val wakeup = PresentationTestWakeup()
        val timerProblem = IllegalStateException("Public timer failure")
        val ownerProblem = IllegalStateException("Public owner failure")
        var closes = 0
        var visible: Int? = null
        var fault: CoreStatus? = null
        val presentation = ForegroundPresentation(AutoCloseable { closes++; throw ownerProblem },
            { 17 }, { 50 }, { CoreStatus.IO_UNCERTAIN }, ui, wakeup,
            { visible = it }, { visible = null; fault = it })
        presentation.requestUpdate()
        ui.runNext()
        assertEquals(17, visible)
        wakeup.cancelProblem = timerProblem
        presentation.requestUpdate()
        assertSame(timerProblem, assertFailsWith<IllegalStateException> { ui.runNext() })
        assertEquals(listOf(ownerProblem), timerProblem.suppressed.toList())
        assertNull(visible)
        assertEquals(CoreStatus.IO_UNCERTAIN, fault)
        assertNull(wakeup.callback)
        assertFalse(presentation.requestUpdate())
        presentation.close()
        assertEquals(1, closes)
    }

    @Test fun failedRendererPreservesOriginalAndRunsAllCleanup() {
        val ui = PresentationTestQueue()
        val wakeup = PresentationTestWakeup()
        val renderProblem = AssertionError("Public renderer failure")
        val closeProblem = IllegalStateException("Public close failure")
        val clearProblem = IllegalArgumentException("Public clear failure")
        var closes = 0
        var cleared = false
        val presentation = ForegroundPresentation(AutoCloseable { closes++; throw closeProblem },
            { 17 }, { 50 }, { CoreStatus.IO_UNCERTAIN }, ui, wakeup,
            { throw renderProblem }, { cleared = true; throw clearProblem })
        presentation.requestUpdate()
        assertSame(renderProblem, assertFailsWith<AssertionError> { ui.runNext() })
        assertEquals(listOf(closeProblem, clearProblem), renderProblem.suppressed.toList())
        assertTrue(cleared)
        assertEquals(1, closes)
        assertNull(wakeup.callback)
        assertFalse(presentation.requestUpdate())
        presentation.close()
    }

    @Test fun fatalSampleFailureClearsAndPropagatesWithoutPublishing() {
        val ui = PresentationTestQueue()
        val problem = AssertionError("Public sample failure")
        var closes = 0
        var fault: CoreStatus? = null
        ForegroundPresentation<Int>(AutoCloseable { closes++ }, { throw problem }, { 50 },
            { error("Fatal errors must not be mapped") }, ui, PresentationTestWakeup(),
            { error("Invalid sample rendered") }, { fault = it }).use {
            it.requestUpdate()
            assertSame(problem, assertFailsWith<AssertionError> { ui.runNext() })
            assertEquals(1, closes)
            assertEquals(CoreStatus.IO_UNCERTAIN, fault)
        }
    }

    @Test fun negativeDelayFailsClosedAndDoesNotPublish() {
        val ui = PresentationTestQueue()
        var closed = false
        var fault: CoreStatus? = null
        ForegroundPresentation(AutoCloseable { closed = true }, { 17 }, { -1 },
            { CoreStatus.IO_UNCERTAIN }, ui, PresentationTestWakeup(),
            { error("Invalid delay rendered") }, { fault = it }).use {
            it.requestUpdate()
            ui.runNext()
            assertTrue(closed)
            assertEquals(CoreStatus.IO_UNCERTAIN, fault)
        }
    }

    @Test fun closeOnAnotherThreadIsRejectedWithoutLosingOwner() {
        val ui = PresentationTestQueue()
        val failure = AtomicReference<Throwable>()
        var closes = 0
        val presentation = ForegroundPresentation(AutoCloseable { closes++ }, { 17 }, { 0 },
            { CoreStatus.IO_UNCERTAIN }, ui, PresentationTestWakeup(), {}, {})
        val worker = Thread {
            try { presentation.close() } catch (problem: Throwable) { failure.set(problem) }
        }
        try {
            worker.start()
            worker.join(5000)
            assertFalse(worker.isAlive)
            assertIs<IllegalStateException>(failure.get())
            assertEquals(0, closes)
            assertTrue(presentation.requestUpdate())
            ui.runNext()
        } finally { presentation.close() }
        assertEquals(1, closes)
    }
}
