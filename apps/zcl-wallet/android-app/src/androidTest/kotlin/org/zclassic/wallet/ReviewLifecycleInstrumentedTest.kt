// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.os.SystemClock
import android.view.WindowManager
import android.widget.Button
import androidx.lifecycle.Lifecycle
import androidx.test.core.app.ActivityScenario
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith
import org.zclassic.wallet.core.CoreStatus
import org.zclassic.wallet.core.Network
import org.zclassic.wallet.core.TransparentAddress
import org.zclassic.wallet.core.UnsignedReview
import org.zclassic.wallet.core.UnsignedReviewFailure
import org.zclassic.wallet.core.Zatoshi

/** Public synthetic reviews in a debug-only Activity; never a custody bypass. */
@RunWith(AndroidJUnit4::class)
class ReviewLifecycleInstrumentedTest {
    private val instrumentation = InstrumentationRegistry.getInstrumentation()
    private data class Displayed(val review: UnsignedReview, val view: ReviewView)
    private fun fixture(name: String) = instrumentation.context.assets.open("review/$name").use { it.readBytes() }
    private fun owner() = UnsignedReview.prepare(listOf(
        UnsignedReview.Funding(fixture("previous0"), 0, 0xffff_ffffL),
        UnsignedReview.Funding(fixture("previous1"), 1, 0xffff_ffffL)), listOf(
        UnsignedReview.Output(TransparentAddress.fromPublicKeyHash(ByteArray(20) { 0x55 }, Network.MAINNET), Zatoshi.of(9000)),
        UnsignedReview.Output(TransparentAddress.fromScriptHash(ByteArray(20) { 0x66 }, Network.MAINNET), Zatoshi.of(1500))),
        Network.MAINNET, 0, 0, Zatoshi.of(500), SystemClock::elapsedRealtime)

    private fun unavailable(view: ReviewView) {
        assertTrue(view.text.startsWith(instrumentation.targetContext.getString(R.string.review_unavailable)))
        assertFalse(view.text.contains("ZCL"))
        assertFalse(view.text.any { it in '0'..'9' })
    }

    private fun empty(scenario: ActivityScenario<WalletReviewFixtureActivity>) {
        scenario.onActivity {
            assertTrue(it.window.attributes.flags and WindowManager.LayoutParams.FLAG_SECURE != 0)
            unavailable(it.findViewById(R.id.transaction_review))
        }
    }

    private fun complete(scenario: ActivityScenario<WalletReviewFixtureActivity>): Displayed {
        val review = owner() // Bounded preparation on the instrumentation worker.
        try {
            scenario.onActivity { assertTrue(it.showFixture(review)) }
            instrumentation.waitForIdleSync()
            var view: ReviewView? = null
            scenario.onActivity {
                val current = it.findViewById<ReviewView>(R.id.transaction_review)
                assertTrue(current.text.startsWith(it.getString(R.string.review_unsigned)))
                assertTrue(current.text.contains("Fee: 0.000005 ZCL"))
                assertTrue(current.text.contains("602c673db0503b48a400414347009ae968b1aaba9b10663bde508e7f8218dc46"))
                view = current
            }
            return Displayed(review, checkNotNull(view))
        } catch (problem: Throwable) {
            review.close()
            throw problem
        }
    }

    private fun cancelled(review: UnsignedReview) {
        assertEquals(CoreStatus.CANCELLED,
            assertThrows(UnsignedReviewFailure::class.java) { review.snapshot() }.status)
        assertEquals(CoreStatus.CANCELLED,
            assertThrows(UnsignedReviewFailure::class.java) { review.unsignedBytes() }.status)
    }

    @Test fun recreationClearsOldViewsAndReleasesTheSingleNativeReviewSlot() {
        ActivityScenario.launch(WalletReviewFixtureActivity::class.java).use { scenario ->
            repeat(3) {
                empty(scenario)
                val previous = complete(scenario)
                try {
                    scenario.recreate()
                    empty(scenario)
                    instrumentation.runOnMainSync { unavailable(previous.view) }
                    cancelled(previous.review)
                } finally { previous.review.close() }
            }
            complete(scenario).review.close()
        }
    }

    @Test fun backgroundRejectsLatePreparedOwnerAndResumeStartsEmpty() {
        ActivityScenario.launch(WalletReviewFixtureActivity::class.java).use { scenario ->
            var activity: WalletReviewFixtureActivity? = null
            scenario.onActivity { activity = it }
            val previous = complete(scenario)
            try {
                scenario.moveToState(Lifecycle.State.CREATED)
                instrumentation.runOnMainSync { unavailable(previous.view) }
                cancelled(previous.review)
                owner().use { late ->
                    instrumentation.runOnMainSync {
                        assertFalse(checkNotNull(activity).showFixture(late))
                        unavailable(previous.view)
                    }
                    cancelled(late)
                }
                scenario.moveToState(Lifecycle.State.RESUMED)
                empty(scenario)
                complete(scenario).review.close()
            } finally { previous.review.close() }
        }
    }

    @Test fun queuedRedrawCannotRepopulateAnExplicitlyClosedScreen() {
        ActivityScenario.launch(WalletReviewFixtureActivity::class.java).use { scenario ->
            owner().use { review ->
                scenario.onActivity {
                    assertTrue(it.showFixture(review))
                    it.closeFixture() // Main executor has not delivered its queued signal yet.
                    unavailable(it.findViewById(R.id.transaction_review))
                }
                instrumentation.waitForIdleSync()
                empty(scenario)
                cancelled(review)
                complete(scenario).review.close()
            }
        }
    }

    @Test fun closeActionCancelsReviewAndNewActivityDoesNotReplay() {
        ActivityScenario.launch(WalletReviewFixtureActivity::class.java).use { scenario ->
            val previous = complete(scenario)
            try {
                scenario.onActivity {
                    val close = it.findViewById<Button>(R.id.review_close)
                    assertTrue(close.filterTouchesWhenObscured)
                    assertTrue(close.performClick())
                    assertTrue(it.isFinishing)
                }
                scenario.moveToState(Lifecycle.State.DESTROYED)
                instrumentation.runOnMainSync { unavailable(previous.view) }
                cancelled(previous.review)
            } finally { previous.review.close() }
        }
        ActivityScenario.launch(WalletReviewFixtureActivity::class.java).use { scenario ->
            instrumentation.waitForIdleSync()
            empty(scenario)
            complete(scenario).review.close()
        }
    }
}
