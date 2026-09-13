// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.os.Parcelable
import android.util.SparseArray
import android.view.ViewGroup
import android.widget.TextView
import androidx.test.core.app.ActivityScenario
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicLong
import java.util.concurrent.atomic.AtomicReference
import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith
import org.zclassic.wallet.core.CoreStatus
import org.zclassic.wallet.core.Network
import org.zclassic.wallet.core.ReadOnlySync
import org.zclassic.wallet.core.TransparentAddress

@RunWith(AndroidJUnit4::class)
class HistoryViewInstrumentedTest {
    private val instrumentation = InstrumentationRegistry.getInstrumentation()
    private val context = instrumentation.targetContext
    private val now = AtomicLong(0)
    private val address = TransparentAddress.fromPublicKeyHash(ByteArray(20), Network.MAINNET)
    private val firstId = "0".repeat(63) + "1"
    private val secondId = "f".repeat(64)
    private fun owner() = ReadOnlySync.withHistory(address, ByteArray(32), now::get)

    private fun complete(sync: ReadOnlySync, empty: Boolean = false, includeHistory: Boolean = true) {
        val attempt = sync.begin()
        for (step in 1..(if (includeHistory) 7 else 6)) {
            attempt.request()
            val frame = if (includeHistory && step == 6) {
                val entries = if (empty) "" else
                    "{\"tx_hash\":\"$firstId\",\"height\":1},{\"tx_hash\":\"$secondId\",\"height\":-1}"
                "{\"id\":6,\"result\":[$entries]}"
            } else instrumentation.context.assets.open("sync/mainnet-${minOf(step, 6)}.json")
                .use { it.readBytes().toString(Charsets.US_ASCII) }
                .let { if (step == 7) it.replaceFirst("\"id\":6", "\"id\":7") else it }
                // Synthetic consistent height claim; this is not a valid block proof.
                .replaceFirst("\"height\":0", "\"height\":1")
            assertEquals(CoreStatus.OK, attempt.reply(frame.toByteArray(Charsets.US_ASCII)))
        }
    }

    private fun unavailable(view: HistoryView) {
        assertTrue(view.text.startsWith(context.getString(R.string.history_unavailable)))
        assertFalse(view.text.contains(firstId))
        assertFalse(view.text.contains(secondId))
        assertFalse(view.text.contains(context.getString(R.string.history_empty_assertion)))
    }

    @Test fun fullAndStaleHistoryRetainUnverifiedLabelsWithoutConfirmationClaims() = owner().use { sync ->
        complete(sync)
        instrumentation.runOnMainSync {
            val view = HistoryView(context)
            view.show(sync.snapshot())
            assertTrue(view.text.startsWith(context.getString(R.string.history_unverified)))
            assertTrue(view.text.contains(context.getString(R.string.history_scope)))
            assertTrue(view.text.contains(firstId) && view.text.contains(secondId))
            assertTrue(view.text.contains("Reported block height: 1"))
            assertTrue(view.text.contains(context.getString(R.string.history_pending_parent)))
            assertFalse(view.text.contains("confirmations") || view.text.contains("ZCL"))
            assertEquals(CoreStatus.IO_FAILURE, sync.begin().fail(CoreStatus.IO_FAILURE))
            view.show(sync.snapshot())
            assertTrue(view.text.startsWith(context.getString(R.string.history_stale)))
            assertTrue(view.text.contains(firstId))
            assertTrue(view.text.contains(context.getString(R.string.history_update_failed)))
            view.showUnavailable(CoreStatus.CANCELLED)
            unavailable(view)
        }
    }

    @Test fun emptyAssertionAbsentQueryAndUnavailableRemainDistinct() {
        owner().use { sync ->
            instrumentation.runOnMainSync {
                val view = HistoryView(context)
                unavailable(view)
                view.show(sync.snapshot())
                unavailable(view)
            }
            complete(sync, empty = true)
            instrumentation.runOnMainSync {
                val view = HistoryView(context)
                view.show(sync.snapshot())
                assertTrue(view.text.startsWith(context.getString(R.string.history_unverified)))
                assertTrue(view.text.contains(context.getString(R.string.history_empty_assertion)))
            }
        }
        ReadOnlySync(address, ByteArray(32), now::get).use { sync ->
            complete(sync, includeHistory = false)
            instrumentation.runOnMainSync {
                val view = HistoryView(context)
                view.show(sync.snapshot())
                unavailable(view)
                assertTrue(view.text.contains(context.getString(R.string.history_not_reported)))
            }
        }
    }

    @Test fun forgedSavedStateAndActualDetachClearTransactionIds() = owner().use { sync ->
        complete(sync)
        ActivityScenario.launch(WalletDisplayFixtureActivity::class.java).use { scenario ->
            scenario.onActivity { activity ->
                val view = activity.findViewById<HistoryView>(R.id.address_history)
                view.show(sync.snapshot())
                val state = SparseArray<Parcelable>()
                view.saveHierarchyState(state)
                assertEquals(0, state.size())
                assertFalse(view.isSaveEnabled || view.freezesText)
                TextView(activity).apply {
                    id = R.id.address_history
                    freezesText = true
                    text = view.text
                }.saveHierarchyState(state)
                assertTrue(state.size() > 0)
                view.restoreHierarchyState(state)
                unavailable(view)
                view.show(sync.snapshot())
                (view.parent as ViewGroup).removeView(view)
                unavailable(view)
            }
        }
        Unit
    }

    @Test fun malformedDisplayValuesClearPriorIdsBeforeRefusal() = owner().use { sync ->
        complete(sync)
        val good = sync.snapshot()
        val report = checkNotNull(good.report)
        val entries = checkNotNull(report.history)
        val bad = listOf(List(17) { entries[0] },
            listOf(entries[0], entries[1].copy(transactionId = "bad\n")),
            listOf(entries[0].copy(reportedHeight = Long.MAX_VALUE)))
        instrumentation.runOnMainSync {
            val view = HistoryView(context)
            for (history in bad) {
                view.show(good)
                assertThrows(IllegalArgumentException::class.java) { view.show(good.copy(report = report.copy(history = history))) }
                unavailable(view)
            }
        }
    }

    @Test fun idleTimerUpdatesHistoryThroughItsForegroundOwner() {
        val sync = owner()
        val delivered = CountDownLatch(2)
        val problem = AtomicReference<CoreStatus>()
        var presentation: BalancePresentation? = null
        var view: HistoryView? = null
        try {
            complete(sync)
            now.set(59_999)
            instrumentation.runOnMainSync {
                val currentView = HistoryView(context)
                view = currentView
                presentation = BalancePresentation(sync, context.mainExecutor, MainQueueBalanceWakeup(),
                    { currentView.show(it); now.set(60_000); delivered.countDown() },
                    { currentView.showUnavailable(it); problem.set(it); delivered.countDown(); delivered.countDown() })
                assertTrue(checkNotNull(presentation).requestUpdate())
            }
            assertTrue(delivered.await(5, TimeUnit.SECONDS))
            assertNull(problem.get())
            instrumentation.runOnMainSync {
                assertTrue(checkNotNull(view).text.startsWith(context.getString(R.string.history_stale)))
                assertTrue(checkNotNull(view).text.contains(firstId))
            }
        } finally {
            instrumentation.runOnMainSync { presentation?.close(); view?.showUnavailable() }
            sync.close()
        }
    }
}
