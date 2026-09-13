// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import java.util.concurrent.atomic.AtomicInteger
import java.util.concurrent.atomic.AtomicLong
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertFalse
import kotlin.test.assertNull
import kotlin.test.assertSame
import kotlin.test.assertTrue
import org.zclassic.wallet.core.CoreStatus
import org.zclassic.wallet.core.Network
import org.zclassic.wallet.core.UnsignedReview
import org.zclassic.wallet.core.UnsignedReviewFailure
import org.zclassic.wallet.core.Zatoshi

class ReviewPresentationTest {
    private val now = AtomicLong(0)
    private val samples = AtomicInteger(0)
    private fun fixture(name: String): ByteArray =
        checkNotNull(javaClass.getResourceAsStream("/review/$name")).use { it.readBytes() }
    private fun owner(clock: () -> Long = { samples.incrementAndGet(); now.get() }) =
        UnsignedReview.open(fixture("draft"), arrayOf(fixture("previous0"), fixture("previous1")),
            Network.MAINNET, Zatoshi.of(500), clock)

    @Test fun queuedDeliveryRechecksInclusiveExpiryAndCoalescesSignals() {
        val ui = PresentationTestQueue()
        val review = owner()
        var fault: CoreStatus? = null
        ReviewPresentation(review, ui, PresentationTestWakeup(),
            { error("Expired review rendered") }, { fault = it }).use { presentation ->
            val before = samples.get()
            repeat(128) { assertTrue(presentation.requestUpdate()) }
            assertEquals(1, ui.callbacks.size)
            assertEquals(before, samples.get())
            now.set(90_000)
            ui.runNext()
            assertEquals(before + 1, samples.get())
            assertEquals(CoreStatus.TIMED_OUT, fault)
            assertFalse(presentation.requestUpdate())
            assertEquals(CoreStatus.CANCELLED,
                assertFailsWith<UnsignedReviewFailure> { review.snapshot() }.status)
        }
        owner().close() // Expired presentation released the single process slot.
    }

    @Test fun earlyWakeupRearmsFromCAndLateDeliveryClearsPreviouslyRenderedData() {
        val ui = PresentationTestQueue()
        val wakeup = PresentationTestWakeup()
        val review = owner()
        var visible: UnsignedReview.Snapshot? = null
        var fault: CoreStatus? = null
        ReviewPresentation(review, ui, wakeup, {
            assertFalse(Thread.holdsLock(review))
            visible = it
        }, { visible = null; fault = it }).use {
            it.requestUpdate()
            ui.runNext()
            assertEquals(90_000L, wakeup.delay)
            assertEquals(500L, checkNotNull(visible).fee.value)
            now.set(89_999)
            wakeup.fire()
            ui.runNext()
            assertEquals(1L, wakeup.delay)
            assertEquals(1L, checkNotNull(visible).remainingMillis)
            wakeup.fire()
            now.set(90_005)
            ui.runNext()
            assertNull(visible)
            assertEquals(CoreStatus.TIMED_OUT, fault)
            assertNull(wakeup.callback)
            assertFalse(it.requestUpdate())
        }
    }

    @Test fun cancelledQueuedOwnerAndCapturedTimerCannotReadOrCancelReplacement() {
        val ui = PresentationTestQueue()
        val wakeup = PresentationTestWakeup()
        var deliveries = 0
        val old = ReviewPresentation(owner(), ui, wakeup, { deliveries++ }, { error("Old failure delivered") })
        try {
            old.requestUpdate()
            ui.runNext()
            val late = checkNotNull(wakeup.callback)
            old.requestUpdate()
            old.close()
            assertNull(wakeup.callback)
            repeat(6) {
                val review = owner()
                ReviewPresentation(review, ui, PresentationTestWakeup(),
                    { deliveries++ }, { error("Replacement failed") }).use { next ->
                    val before = samples.get()
                    late.run()
                    if (it == 0) ui.runNext() // Already queued original redraw.
                    old.close()
                    assertFalse(old.requestUpdate())
                    assertEquals(before, samples.get())
                    next.requestUpdate()
                    ui.runNext()
                    assertEquals(before + 1, samples.get())
                }
            }
            assertEquals(7, deliveries)
        } finally { old.close() }
    }

    @Test fun backwardsClockClearsTheReviewAndReleasesNativeSlot() {
        val ui = PresentationTestQueue()
        now.set(100)
        var fault: CoreStatus? = null
        ReviewPresentation(owner(), ui, PresentationTestWakeup(),
            { error("Rollback review rendered") }, { fault = it }).use {
            it.requestUpdate()
            now.set(99)
            ui.runNext()
            assertEquals(CoreStatus.CANCELLED, fault)
            assertFalse(it.requestUpdate())
        }
        owner().close()
    }

    @Test fun failedRenderingClearsTheScreenAndReleasesNativeSlot() {
        val ui = PresentationTestQueue()
        val wakeup = PresentationTestWakeup()
        val problem = IllegalStateException("Public review render failure")
        var fault: CoreStatus? = null
        ReviewPresentation(owner(), ui, wakeup, { throw problem }, { fault = it }).use {
            it.requestUpdate()
            assertSame(problem, assertFailsWith<IllegalStateException> { ui.runNext() })
            assertEquals(CoreStatus.IO_UNCERTAIN, fault)
            assertNull(wakeup.callback)
            assertFalse(it.requestUpdate())
        }
        owner().close()
    }

    @Test fun rejectedExpiryWakeupRefusesToPublishAndReleasesNativeSlot() {
        val ui = PresentationTestQueue()
        var fault: CoreStatus? = null
        ReviewPresentation(owner(), ui, PresentationTestWakeup().apply { rejecting = true },
            { error("Unscheduled review rendered") }, { fault = it }).use {
            it.requestUpdate()
            ui.runNext()
            assertEquals(CoreStatus.RESOURCE_EXHAUSTED, fault)
            assertFalse(it.requestUpdate())
        }
        owner().close()
    }
}
