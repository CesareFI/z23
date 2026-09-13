// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.os.Parcelable
import android.util.SparseArray
import android.widget.TextView
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicLong
import java.util.concurrent.atomic.AtomicReference
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertThrows
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.zclassic.wallet.core.CoreStatus
import org.zclassic.wallet.core.Network
import org.zclassic.wallet.core.ReadOnlySync
import org.zclassic.wallet.core.TransparentAddress

/** Display-only public fixtures; no wallet record, authentication or endpoint. */
@RunWith(AndroidJUnit4::class)
class BalanceViewInstrumentedTest {
    private val instrumentation = InstrumentationRegistry.getInstrumentation()
    private val context = instrumentation.targetContext
    private val now = AtomicLong(0)
    private fun owner() = ReadOnlySync(
        TransparentAddress.fromPublicKeyHash(ByteArray(20), Network.MAINNET), ByteArray(32), now::get)

    private fun complete(sync: ReadOnlySync, zero: Boolean = false) {
        val attempt = sync.begin()
        for (step in 1..6) {
            attempt.request()
            val frame = if (zero && step == 5)
                "{\"id\":5,\"result\":{\"confirmed\":0,\"unconfirmed\":0}}".toByteArray(Charsets.US_ASCII)
            else instrumentation.context.assets.open("sync/mainnet-$step.json").use { it.readBytes() }
            assertEquals(CoreStatus.OK, attempt.reply(frame))
        }
    }

    private fun unavailable(view: BalanceView) {
        assertTrue(view.text.startsWith(context.getString(R.string.balance_unavailable)))
        assertFalse(view.text.contains("ZCL"))
        assertFalse(view.text.any { it in '0'..'9' })
    }

    @Test fun completeAndOfflineReportsKeepExactAmountsAndTrustLabels() = owner().use { sync ->
        complete(sync)
        instrumentation.runOnMainSync {
            val view = BalanceView(context)
            view.show(sync.snapshot())
            assertTrue(view.text.startsWith(context.getString(R.string.balance_unverified)))
            assertTrue(view.text.contains("Reported total: 0.00000993 ZCL"))
            assertTrue(view.text.contains("Reported confirmed: 0.00001 ZCL"))
            assertTrue(view.text.contains("Pending change: -0.00000007 ZCL"))
            assertTrue(view.text.contains(context.getString(R.string.balance_report_scope)))
            assertEquals(CoreStatus.IO_FAILURE, sync.begin().fail(CoreStatus.IO_FAILURE))
            view.show(sync.snapshot())
            assertTrue(view.text.startsWith(context.getString(R.string.balance_stale)))
            assertTrue(view.text.contains("0.00000993 ZCL"))
            assertTrue(view.text.contains(context.getString(R.string.balance_update_failed)))
            view.showUnavailable(CoreStatus.CANCELLED)
            unavailable(view)
            assertTrue(view.text.contains(context.getString(R.string.balance_cancelled)))
        }
    }

    @Test fun unavailableAndReportedZeroRemainDifferent() = owner().use { sync ->
        instrumentation.runOnMainSync {
            val view = BalanceView(context)
            unavailable(view)
            view.show(sync.snapshot())
            unavailable(view)
            sync.begin()
            view.show(sync.snapshot())
            unavailable(view)
            assertTrue(view.text.contains(context.getString(R.string.balance_updating)))
        }
        sync.close()
        owner().use { zero ->
            complete(zero, zero = true)
            instrumentation.runOnMainSync {
                val view = BalanceView(context)
                view.show(zero.snapshot())
                assertTrue(view.text.startsWith(context.getString(R.string.balance_unverified)))
                assertTrue(view.text.contains("Reported total: 0 ZCL"))
                assertTrue(view.text.contains("Pending change: 0 ZCL"))
            }
        }
    }

    @Test fun savedHierarchyCannotRestoreAnyReportedAmount() = owner().use { sync ->
        complete(sync)
        instrumentation.runOnMainSync {
            val view = BalanceView(context)
            view.show(sync.snapshot())
            val saved = SparseArray<Parcelable>()
            view.saveHierarchyState(saved)
            assertEquals(0, saved.size())
            assertFalse(view.isSaveEnabled)
            assertFalse(view.freezesText)
            // Model an earlier version's ordinary TextView state under this ID.
            val oldView = TextView(context).apply {
                id = R.id.address_balance
                freezesText = true
                text = view.text
            }
            oldView.saveHierarchyState(saved)
            assertTrue(saved.size() > 0)
            val restored = BalanceView(context)
            restored.restoreHierarchyState(saved)
            unavailable(restored)
            view.restoreHierarchyState(saved)
            unavailable(view)
        }
    }

    @Test fun invalidDisplayInputClearsAnEarlierAmountBeforeRefusing() = owner().use { sync ->
        complete(sync)
        val snapshot = sync.snapshot()
        val malformed = snapshot.copy(report = checkNotNull(snapshot.report).copy(pendingDelta = Long.MIN_VALUE))
        instrumentation.runOnMainSync {
            val view = BalanceView(context)
            view.show(snapshot)
            assertThrows(IllegalArgumentException::class.java) { view.show(malformed) }
            unavailable(view)
        }
    }

    @Test fun idleTimerUpdatesActualViewToOutdatedUnverifiedState() {
        val sync = owner()
        val ready = CountDownLatch(2)
        val problem = AtomicReference<CoreStatus>()
        var presentation: BalancePresentation? = null
        var view: BalanceView? = null
        try {
            complete(sync)
            now.set(59999)
            instrumentation.runOnMainSync {
                val currentView = BalanceView(context)
                view = currentView
                val current = BalancePresentation(sync, context.mainExecutor, MainQueueBalanceWakeup(),
                    { currentView.show(it); now.set(60000); ready.countDown() },
                    { currentView.showUnavailable(it); problem.set(it); ready.countDown(); ready.countDown() })
                presentation = current
                assertTrue(current.requestUpdate())
            }
            assertTrue(ready.await(5, TimeUnit.SECONDS))
            assertEquals(null, problem.get())
            instrumentation.runOnMainSync {
                val text = checkNotNull(view).text
                assertTrue(text.startsWith(context.getString(R.string.balance_stale)))
                assertTrue(text.contains("0.00000993 ZCL"))
            }
        } finally {
            instrumentation.runOnMainSync { presentation?.close(); view?.showUnavailable() }
            sync.close()
        }
    }
}
