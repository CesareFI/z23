// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.text.Editable
import android.text.TextWatcher
import android.view.View
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import java.util.concurrent.Executor
import java.util.concurrent.atomic.AtomicReference
import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith
import org.zclassic.wallet.core.CoreStatus
import org.zclassic.wallet.core.Network
import org.zclassic.wallet.core.UnsignedReview
import org.zclassic.wallet.core.UnsignedReviewFailure
import org.zclassic.wallet.core.Zatoshi

/** Public unsigned fixtures and real C review lifetimes; no wallet or consent. */
@RunWith(AndroidJUnit4::class)
class ReviewRenderFailureInstrumentedTest {
    private val instrumentation = InstrumentationRegistry.getInstrumentation()
    private fun fixture(name: String) = instrumentation.context.assets.open("review/$name").use { it.readBytes() }
    private fun owner(clock: () -> Long = { 0L }) = UnsignedReview.open(fixture("draft"),
        arrayOf(fixture("previous0"), fixture("previous1")), Network.MAINNET, Zatoshi.of(500), clock)

    private fun onMain(action: () -> Unit) {
        val failure = AtomicReference<Throwable?>()
        instrumentation.runOnMainSync {
            try { action() } catch (problem: Throwable) { failure.set(problem) }
        }
        failure.get()?.let { throw it }
    }

    private abstract class Watcher : TextWatcher {
        override fun beforeTextChanged(text: CharSequence?, start: Int, count: Int, after: Int) = Unit
        override fun onTextChanged(text: CharSequence?, start: Int, before: Int, count: Int) = Unit
        override fun afterTextChanged(text: Editable?) = Unit
    }

    @Test fun unavailableClearFailureConcealsTheEarlierTransactionForEveryStatus() = owner().use { review ->
        val snapshot = review.snapshot()
        onMain {
            val view = ReviewView(instrumentation.targetContext)
            for (status in listOf(CoreStatus.OK, CoreStatus.TIMED_OUT, CoreStatus.CANCELLED, CoreStatus.IO_UNCERTAIN)) {
                view.show(snapshot)
                val problem = IllegalStateException("Public synthetic review clear refusal")
                val refusal = object : Watcher() {
                    override fun beforeTextChanged(text: CharSequence?, start: Int, count: Int, after: Int) {
                        throw problem
                    }
                }
                view.addTextChangedListener(refusal)
                try {
                    assertSame(problem, assertThrows(IllegalStateException::class.java) { view.showUnavailable(status) })
                    assertTrue(view.text.contains(snapshot.transactionId))
                    assertEquals(View.INVISIBLE, view.visibility)
                } finally { view.removeTextChangedListener(refusal); view.showUnavailable(status) }
                assertEquals(View.VISIBLE, view.visibility)
                assertFalse(view.text.contains(snapshot.transactionId))
            }
        }
    }

    @Test fun failedFinalRenderConcealsCopiedTransactionAndPreservesCleanupFailure() = owner().use { review ->
        val snapshot = review.snapshot()
        onMain {
            val view = ReviewView(instrumentation.targetContext)
            val problem = OutOfMemoryError("Public synthetic review post-render refusal")
            val cleanup = IllegalStateException("Public synthetic review cleanup refusal")
            var rendered = false
            val refusal = object : Watcher() {
                override fun beforeTextChanged(text: CharSequence?, start: Int, count: Int, after: Int) {
                    if (rendered) throw cleanup
                }
                override fun afterTextChanged(text: Editable?) {
                    if (text?.contains(snapshot.transactionId) == true) { rendered = true; throw problem }
                }
            }
            view.addTextChangedListener(refusal)
            try {
                assertSame(problem, assertThrows(OutOfMemoryError::class.java) { view.show(snapshot) })
                assertEquals(listOf(cleanup), problem.suppressed.toList())
                assertTrue(view.text.contains(snapshot.transactionId))
                assertEquals(View.INVISIBLE, view.visibility)
            } finally { view.removeTextChangedListener(refusal); view.showUnavailable() }
            view.show(snapshot)
            assertEquals(View.VISIBLE, view.visibility)
            assertTrue(view.text.contains(snapshot.transactionId))
        }
    }

    @Test fun textCallbacksCannotObserveAnIncompleteVisibleReview() = owner().use { review ->
        val snapshot = review.snapshot()
        onMain {
            val view = ReviewView(instrumentation.targetContext)
            val visibility = mutableListOf<Int>()
            val observer = object : Watcher() {
                override fun beforeTextChanged(text: CharSequence?, start: Int, count: Int, after: Int) {
                    visibility.add(view.visibility)
                }
                override fun afterTextChanged(text: Editable?) { visibility.add(view.visibility) }
            }
            view.addTextChangedListener(observer)
            try {
                view.show(snapshot)
                assertEquals(View.VISIBLE, view.visibility)
                view.showUnavailable(CoreStatus.TIMED_OUT)
                assertEquals(View.VISIBLE, view.visibility)
                assertTrue(visibility.isNotEmpty())
                assertTrue("Text callback observed an incomplete visible review", visibility.all { it == View.INVISIBLE })
            } finally { view.removeTextChangedListener(observer); view.showUnavailable() }
        }
    }

    private class Wakeup : BalanceWakeup {
        var callback: Runnable? = null
        override fun replace(delayMillis: Long, callback: Runnable): Boolean {
            assertEquals(90_000L, delayMillis)
            this.callback = callback
            return true
        }
        override fun cancel() { callback = null }
    }

    @Test fun realNativeExpiryClosesItsOwnerEvenWhenTheDisplayClearRefuses() {
        var now = 0L
        owner { now }.use { review ->
            val snapshot = review.snapshot()
            onMain {
                val view = ReviewView(instrumentation.targetContext)
                var queued: Runnable? = null
                val wakeup = Wakeup()
                val ui = Executor { assertNull(queued); queued = it }
                fun drain() { val current = checkNotNull(queued); queued = null; current.run() }
                val presentation = ReviewPresentation(review, ui, wakeup, view::show, view::showUnavailable)
                val problem = IllegalStateException("Public synthetic expired review clear refusal")
                val refusal = object : Watcher() {
                    override fun beforeTextChanged(text: CharSequence?, start: Int, count: Int, after: Int) {
                        throw problem
                    }
                }
                try {
                    assertTrue(presentation.requestUpdate())
                    drain()
                    view.addTextChangedListener(refusal)
                    now = 90_000L
                    checkNotNull(wakeup.callback).run()
                    assertSame(problem, assertThrows(IllegalStateException::class.java) { drain() })
                    assertFalse(presentation.requestUpdate())
                    assertNull(queued)
                    assertNull(wakeup.callback)
                    assertEquals(CoreStatus.CANCELLED,
                        assertThrows(UnsignedReviewFailure::class.java) { review.snapshot() }.status)
                    owner().close() // The one native review slot was actually returned.
                    assertTrue(view.text.contains(snapshot.transactionId))
                    assertEquals(View.INVISIBLE, view.visibility)
                } finally {
                    view.removeTextChangedListener(refusal)
                    presentation.close()
                    view.showUnavailable()
                }
            }
        }
    }
}
