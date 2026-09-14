// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.text.Editable
import android.text.TextWatcher
import android.view.View
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import java.util.concurrent.atomic.AtomicReference
import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith
import org.zclassic.wallet.core.CoreStatus
import org.zclassic.wallet.core.ReadOnlySync
import org.zclassic.wallet.core.Zatoshi

/** Public display markers and controlled TextView failures only. No endpoint,
 * wallet, key, authentication, saved state or real memory exhaustion. */
@RunWith(AndroidJUnit4::class)
class ReportRenderingFailureInstrumentedTest {
    private val instrumentation = InstrumentationRegistry.getInstrumentation()
    private val transactionId = "4".repeat(64)

    private fun snapshot() = ReadOnlySync.Snapshot(ReadOnlySync.Freshness.UNVERIFIED,
        false, CoreStatus.OK, 0,
        ReadOnlySync.Report(Zatoshi.of(1000), -7, Zatoshi.of(993), 0,
            listOf(ReadOnlySync.HistoryEntry(transactionId, 0))), 60_000)

    private abstract class Watcher : TextWatcher {
        override fun onTextChanged(text: CharSequence?, start: Int, before: Int, count: Int) = Unit
        override fun afterTextChanged(text: Editable?) = Unit
    }

    private fun onMain(action: () -> Unit) {
        val failure = AtomicReference<Throwable?>()
        instrumentation.runOnMainSync {
            try { action() } catch (problem: Throwable) { failure.set(problem) }
        }
        failure.get()?.let { throw it }
    }

    private fun assertHidden(balance: BalanceView, history: HistoryView) {
        assertEquals("Failed balance must not remain visible", View.INVISIBLE, balance.visibility)
        assertEquals("Failed history must not remain visible", View.INVISIBLE, history.visibility)
    }

    @Test fun failedInitialClearRetiresBothEarlierReports() = onMain {
        val balance = BalanceView(instrumentation.targetContext)
        val history = HistoryView(instrumentation.targetContext)
        val views = ReadOnlyReportViews(balance, history)
        for (target in listOf(balance, history)) {
            views.show(snapshot())
            val problem = OutOfMemoryError("Synthetic public initial text-clear failure")
            val listener = object : Watcher() {
                private var fired = false
                override fun beforeTextChanged(text: CharSequence?, start: Int, count: Int, after: Int) {
                    if (!fired) { fired = true; throw problem }
                }
            }
            target.addTextChangedListener(listener)
            try {
                assertSame(problem, assertThrows(OutOfMemoryError::class.java) { views.show(snapshot()) })
                assertTrue("Earlier balance survived failed initial clearing", balance.text.isEmpty())
                assertTrue("Earlier history survived failed initial clearing", history.text.isEmpty())
                assertHidden(balance, history)
            } finally {
                target.removeTextChangedListener(listener)
                views.showUnavailable()
            }
        }
    }

    @Test fun persistentlyRefusedClearStillHidesBothReportsAndClearsItsPeer() = onMain {
        val balance = BalanceView(instrumentation.targetContext)
        val history = HistoryView(instrumentation.targetContext)
        val views = ReadOnlyReportViews(balance, history)
        for (target in listOf(balance, history)) {
            views.show(snapshot())
            val problem = IllegalStateException("Synthetic public persistent clear refusal")
            val listener = object : Watcher() {
                override fun beforeTextChanged(text: CharSequence?, start: Int, count: Int, after: Int) {
                    throw problem
                }
            }
            target.addTextChangedListener(listener)
            try {
                assertSame(problem, assertThrows(IllegalStateException::class.java) { views.showUnavailable() })
                assertHidden(balance, history)
                assertTrue(if (target === balance) history.text.isEmpty() else balance.text.isEmpty())
            } finally {
                target.removeTextChangedListener(listener)
                views.showUnavailable()
            }
            assertEquals(View.VISIBLE, balance.visibility)
            assertEquals(View.VISIBLE, history.visibility)
            assertFalse(balance.text.contains("ZCL"))
            assertFalse(history.text.contains(transactionId))
        }
    }

    @Test fun failedPartialRenderPreservesBothCleanupFailuresAndHidesThePair() = onMain {
        val balance = BalanceView(instrumentation.targetContext)
        val history = HistoryView(instrumentation.targetContext)
        val views = ReadOnlyReportViews(balance, history)
        views.show(snapshot())
        val problem = OutOfMemoryError("Synthetic public history rendering failure")
        val balanceCleanup = IllegalStateException("Synthetic public balance cleanup failure")
        val historyCleanup = IllegalArgumentException("Synthetic public history cleanup failure")
        var renderingFailed = false
        val balanceListener = object : Watcher() {
            override fun beforeTextChanged(text: CharSequence?, start: Int, count: Int, after: Int) {
                if (renderingFailed) throw balanceCleanup
            }
        }
        val historyListener = object : Watcher() {
            override fun beforeTextChanged(text: CharSequence?, start: Int, count: Int, after: Int) {
                if (renderingFailed) throw historyCleanup
                if (after > 0) { renderingFailed = true; throw problem }
            }
        }
        balance.addTextChangedListener(balanceListener)
        history.addTextChangedListener(historyListener)
        try {
            assertSame(problem, assertThrows(OutOfMemoryError::class.java) { views.show(snapshot()) })
            assertHidden(balance, history)
            assertEquals(listOf(balanceCleanup), problem.suppressed.toList())
            assertEquals(listOf(historyCleanup), balanceCleanup.suppressed.toList())
        } finally {
            balance.removeTextChangedListener(balanceListener)
            history.removeTextChangedListener(historyListener)
            views.showUnavailable()
        }
        views.show(snapshot())
        assertEquals(View.VISIBLE, balance.visibility)
        assertEquals(View.VISIBLE, history.visibility)
        assertTrue(balance.text.contains("0.00000993 ZCL"))
        assertTrue(history.text.contains(transactionId))
        views.showUnavailable()
    }

    @Test fun neitherReportIsPublishedBeforeBothTextUpdatesFinish() = onMain {
        val balance = BalanceView(instrumentation.targetContext)
        val history = HistoryView(instrumentation.targetContext)
        val views = ReadOnlyReportViews(balance, history)
        val observer = object : Watcher() {
            override fun beforeTextChanged(text: CharSequence?, start: Int, count: Int, after: Int) {
                assertHidden(balance, history)
            }
        }
        balance.addTextChangedListener(observer)
        history.addTextChangedListener(observer)
        try {
            views.show(snapshot())
            assertEquals(View.VISIBLE, balance.visibility)
            assertEquals(View.VISIBLE, history.visibility)
            views.showUnavailable(CoreStatus.TIMED_OUT)
            assertEquals(View.VISIBLE, balance.visibility)
            assertEquals(View.VISIBLE, history.visibility)
        } finally {
            balance.removeTextChangedListener(observer)
            history.removeTextChangedListener(observer)
            views.showUnavailable()
        }
    }
}
