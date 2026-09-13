// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.os.Parcelable
import android.util.SparseArray
import android.view.View
import android.widget.TextView
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicLong
import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith
import org.zclassic.wallet.core.CoreStatus
import org.zclassic.wallet.core.Network
import org.zclassic.wallet.core.UnsignedReview
import org.zclassic.wallet.core.Zatoshi

/** Synthetic public transactions only. This qualifies display, not spending. */
@RunWith(AndroidJUnit4::class)
class ReviewViewInstrumentedTest {
    private val instrumentation = InstrumentationRegistry.getInstrumentation()
    private val context = instrumentation.targetContext
    private val now = AtomicLong(0)
    private fun fixture(name: String) = instrumentation.context.assets.open("review/$name").use { it.readBytes() }
    private fun owner(network: Network = Network.MAINNET) = UnsignedReview.open(fixture("draft"),
        arrayOf(fixture("previous0"), fixture("previous1")), network, Zatoshi.of(500), now::get)
    private fun unavailable(view: ReviewView) {
        assertTrue(view.text.startsWith(context.getString(R.string.review_unavailable)))
        assertFalse(view.text.contains("ZCL"))
        assertFalse(view.text.any { it in '0'..'9' })
    }

    @Test fun fullDestinationsExactAmountsAndTrustLabelsOnBothNetworks() {
        for (network in Network.entries) owner(network).use { review ->
            val snapshot = review.snapshot()
            instrumentation.runOnMainSync {
                val view = ReviewView(context)
                unavailable(view)
                view.show(snapshot)
                val text = view.text.toString()
                assertTrue(text.startsWith(context.getString(R.string.review_unsigned)))
                assertTrue(text.contains(context.getString(if (network == Network.MAINNET)
                    R.string.network_mainnet else R.string.network_testnet)))
                assertTrue(text.contains(context.getString(R.string.review_funding_notice)))
                assertTrue(text.contains(context.getString(R.string.review_sending_notice)))
                val first = "Output 1\n${snapshot.outputs.first().address().encoded}\n0.00009 ZCL"
                val last = "Output 2\n${snapshot.outputs.last().address().encoded}\n0.000015 ZCL"
                assertTrue(text.contains(first))
                assertTrue(text.contains(last))
                assertTrue(text.indexOf(first) < text.indexOf(last))
                assertTrue(text.contains("Total input value: 0.00011 ZCL"))
                assertTrue(text.contains("Total output value: 0.000105 ZCL"))
                assertTrue(text.contains("Fee: 0.000005 ZCL"))
                assertTrue(text.contains("Fee limit: 0.000005 ZCL"))
                assertTrue(text.contains("602c673db0503b48a400414347009ae968b1aaba9b10663bde508e7f8218dc46"))
                assertTrue(text.contains("Unsigned size (bytes): 177"))
                assertTrue(text.contains("Sequence (raw): 4294967295"))
                snapshot.inputs.forEach { input ->
                    assertTrue(text.contains(input.previousTransactionId))
                    assertTrue(text.contains("Funding address: ${input.destination.address().encoded}"))
                }
                assertFalse(text.contains("Change:"))
                assertFalse(view.hasOnClickListeners())
            }
        }
    }

    @Test fun rawUnsignedFieldsDoNotBecomeNegativeOrFinalityLabels() = owner().use { review ->
        val original = review.snapshot()
        // Formatting-only boundary fixture; no modified snapshot can authorize signing.
        val snapshot = original.copy(lockTime = 0xffff_ffffL, expiryHeight = 499_999_999L,
            inputs = original.inputs.map { it.copy(previousIndex = 0x8000_0000L, sequence = 0x8000_0001L) })
        instrumentation.runOnMainSync {
            val view = ReviewView(context)
            view.show(snapshot)
            assertTrue(view.text.contains("Lock time (raw): 4294967295"))
            assertTrue(view.text.contains("Expiry height (raw): 499999999"))
            assertTrue(view.text.contains("Output index: 2147483648"))
            assertTrue(view.text.contains("Sequence (raw): 2147483649"))
            assertFalse(view.text.contains("final", ignoreCase = true))
        }
    }

    @Test fun closedExpiredAndFailedStatesRemoveEveryTransactionField() = owner().use { review ->
        val snapshot = review.snapshot()
        instrumentation.runOnMainSync {
            val view = ReviewView(context)
            for ((status, reason) in listOf(CoreStatus.OK to R.string.review_waiting,
                CoreStatus.CANCELLED to R.string.review_cancelled, CoreStatus.TIMED_OUT to R.string.review_expired,
                CoreStatus.IO_UNCERTAIN to R.string.review_failed)) {
                view.show(snapshot)
                view.showUnavailable(status)
                unavailable(view)
                assertTrue(view.text.contains(context.getString(reason)))
                assertFalse(view.text.contains(snapshot.transactionId))
            }
        }
    }

    @Test fun oldFrameworkHierarchyCannotRestoreReviewText() = owner().use { review ->
        val snapshot = review.snapshot()
        instrumentation.runOnMainSync {
            val view = ReviewView(context)
            view.show(snapshot)
            val saved = SparseArray<Parcelable>()
            view.saveHierarchyState(saved)
            assertEquals(0, saved.size())
            assertFalse(view.isSaveEnabled)
            assertFalse(view.freezesText)
            assertEquals(View.IMPORTANT_FOR_AUTOFILL_NO_EXCLUDE_DESCENDANTS, view.importantForAutofill)
            assertEquals(View.IMPORTANT_FOR_CONTENT_CAPTURE_NO_EXCLUDE_DESCENDANTS, view.importantForContentCapture)
            val old = TextView(context).apply {
                id = R.id.transaction_review
                freezesText = true
                text = view.text
            }
            old.saveHierarchyState(saved)
            assertTrue(saved.size() > 0)
            val restored = ReviewView(context)
            restored.restoreHierarchyState(saved)
            unavailable(restored)
            view.restoreHierarchyState(saved)
            unavailable(view)
        }
    }

    @Test fun malformedFormattingClearsPreviousAndPartialTransactionDetails() = owner().use { review ->
        val original = review.snapshot()
        val badDestination = original.outputs.last().copy(hashHex = "gg".repeat(20))
        val variants = listOf(original.copy(transactionId = "f".repeat(65)),
            original.copy(transactionId = "z".repeat(64)), original.copy(lockTime = -1),
            original.copy(expiryHeight = 0x1_0000_0000L), original.copy(serializedSize = 1926),
            original.copy(remainingMillis = 0), original.copy(inputs = emptyList()),
            original.copy(outputs = List(17) { original.outputs.first() }),
            original.copy(outputs = listOf(original.outputs.first(), badDestination)),
            original.copy(outputs = listOf(original.outputs.first().copy(network = Network.TESTNET))),
            original.copy(inputs = listOf(original.inputs.last().copy(previousTransactionId = "bad"))))
        instrumentation.runOnMainSync {
            val view = ReviewView(context)
            variants.forEach {
                view.show(original)
                assertThrows(IllegalArgumentException::class.java) { view.show(it) }
                unavailable(view)
                assertFalse(view.text.contains(original.outputs.first().address().encoded))
            }
        }
    }

    @Test fun mainQueueExpiryClearsActualViewAndReleasesNativeReview() {
        val review = owner()
        val ready = CountDownLatch(2)
        var presentation: ReviewPresentation? = null
        var view: ReviewView? = null
        try {
            now.set(89_999)
            instrumentation.runOnMainSync {
                val currentView = ReviewView(context)
                view = currentView
                presentation = ReviewPresentation(review, context.mainExecutor, MainQueueBalanceWakeup(),
                    { currentView.show(it); now.set(90_000); ready.countDown() },
                    { currentView.showUnavailable(it); ready.countDown() })
                assertTrue(checkNotNull(presentation).requestUpdate())
            }
            assertTrue(ready.await(5, TimeUnit.SECONDS))
            instrumentation.runOnMainSync {
                unavailable(checkNotNull(view))
                assertTrue(checkNotNull(view).text.contains(context.getString(R.string.review_expired)))
                assertFalse(checkNotNull(presentation).requestUpdate())
            }
            owner().close()
        } finally {
            instrumentation.runOnMainSync { presentation?.close(); view?.showUnavailable() }
            review.close()
        }
    }
}
