// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import androidx.lifecycle.Lifecycle
import androidx.test.core.app.ActivityScenario
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith
import org.zclassic.wallet.core.CoreStatus
import org.zclassic.wallet.core.ReadOnlySync
import org.zclassic.wallet.core.ReadOnlySyncFailure
import org.zclassic.wallet.core.Zatoshi

@RunWith(AndroidJUnit4::class)
class HistoryLifecycleInstrumentedTest {
    private val instrumentation = InstrumentationRegistry.getInstrumentation()
    private val transactionId = "c".repeat(64)
    private data class Views(val balance: BalanceView, val history: HistoryView)

    private fun empty(views: Views) {
        val context = instrumentation.targetContext
        assertTrue(views.balance.text.startsWith(context.getString(R.string.balance_unavailable)))
        assertTrue(views.history.text.startsWith(context.getString(R.string.history_unavailable)))
        assertFalse(views.balance.text.any { it in '0'..'9' })
        assertFalse(views.history.text.contains(transactionId))
    }

    private fun empty(scenario: ActivityScenario<WalletDisplayFixtureActivity>) {
        scenario.onActivity { empty(Views(it.findViewById(R.id.address_balance), it.findViewById(R.id.address_history))) }
    }

    private fun begin(scenario: ActivityScenario<WalletDisplayFixtureActivity>): ReadOnlySync.Attempt {
        var attempt: ReadOnlySync.Attempt? = null
        scenario.onActivity { attempt = it.beginHistoryFixture() }
        return checkNotNull(attempt)
    }

    private fun complete(scenario: ActivityScenario<WalletDisplayFixtureActivity>): Views {
        val attempt = begin(scenario)
        for (step in 1..7) {
            attempt.request()
            val frame = if (step == 6) "{\"id\":6,\"result\":[{\"tx_hash\":\"$transactionId\",\"height\":0}]}"
            else instrumentation.context.assets.open("sync/mainnet-${minOf(step, 6)}.json")
                .use { it.readBytes().toString(Charsets.US_ASCII) }
                .let { if (step == 7) it.replaceFirst("\"id\":6", "\"id\":7") else it }
            assertEquals(CoreStatus.OK, attempt.reply(frame.toByteArray(Charsets.US_ASCII)))
        }
        scenario.onActivity { assertTrue(it.redrawFixture()) }
        instrumentation.waitForIdleSync()
        var views: Views? = null
        scenario.onActivity {
            val current = Views(it.findViewById(R.id.address_balance), it.findViewById(R.id.address_history))
            assertTrue(current.balance.text.contains("0.00000993 ZCL"))
            assertTrue(current.history.text.startsWith(it.getString(R.string.history_unverified)))
            assertTrue(current.history.text.contains(transactionId))
            views = current
        }
        return checkNotNull(views)
    }

    private fun cancelled(attempt: ReadOnlySync.Attempt) {
        assertEquals(CoreStatus.CANCELLED, attempt.reply(byteArrayOf()))
        assertEquals(CoreStatus.CANCELLED,
            assertThrows(ReadOnlySyncFailure::class.java) { attempt.request() }.status)
    }

    @Test fun recreationClearsBothViewsAndReleasesHistoryOwners() {
        ActivityScenario.launch(WalletDisplayFixtureActivity::class.java).use { scenario ->
            repeat(5) {
                empty(scenario)
                val old = complete(scenario)
                val late = begin(scenario)
                scenario.recreate()
                empty(scenario)
                instrumentation.runOnMainSync { empty(old) }
                cancelled(late)
            }
        }
    }

    @Test fun backgroundClosesHistoryAttemptAndResumesWithAnEmptyOwner() {
        ActivityScenario.launch(WalletDisplayFixtureActivity::class.java).use { scenario ->
            val old = complete(scenario)
            val late = begin(scenario)
            scenario.moveToState(Lifecycle.State.CREATED)
            instrumentation.runOnMainSync { empty(old) }
            cancelled(late)
            scenario.moveToState(Lifecycle.State.RESUMED)
            empty(scenario)
            complete(scenario)
        }
    }

    @Test fun failedCompositeRenderingClearsBothEarlierAndPartialValues() {
        ActivityScenario.launch(WalletDisplayFixtureActivity::class.java).use { scenario ->
            scenario.onActivity {
                val balance = it.findViewById<BalanceView>(R.id.address_balance)
                val history = it.findViewById<HistoryView>(R.id.address_history)
                val views = ReadOnlyReportViews(balance, history)
                val report = ReadOnlySync.Report(Zatoshi.of(1000), -7, Zatoshi.of(993), 0,
                    listOf(ReadOnlySync.HistoryEntry(transactionId, 0)))
                val good = ReadOnlySync.Snapshot(ReadOnlySync.Freshness.UNVERIFIED,
                    false, CoreStatus.OK, 0, report, 60_000)
                val invalid = listOf(report.copy(pendingDelta = Long.MIN_VALUE),
                    report.copy(history = listOf(ReadOnlySync.HistoryEntry("invalid", 0))))
                for (bad in invalid) {
                    views.show(good)
                    assertTrue(balance.text.contains("ZCL") && history.text.contains(transactionId))
                    assertThrows(IllegalArgumentException::class.java) { views.show(good.copy(report = bad)) }
                    assertTrue(balance.text.isEmpty() && history.text.isEmpty())
                }
            }
        }
    }
}
