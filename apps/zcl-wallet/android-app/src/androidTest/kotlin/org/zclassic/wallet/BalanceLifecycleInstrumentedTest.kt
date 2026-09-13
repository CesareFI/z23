// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.view.WindowManager
import androidx.lifecycle.Lifecycle
import androidx.test.core.app.ActivityScenario
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertThrows
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.zclassic.wallet.core.CoreStatus
import org.zclassic.wallet.core.ReadOnlySync
import org.zclassic.wallet.core.ReadOnlySyncFailure

/** Actual lifecycle of a debug public fixture host; never a custody-flow bypass. */
@RunWith(AndroidJUnit4::class)
class BalanceLifecycleInstrumentedTest {
    private val instrumentation = InstrumentationRegistry.getInstrumentation()

    private fun unavailable(view: BalanceView) {
        assertTrue(view.text.startsWith(instrumentation.targetContext.getString(R.string.balance_unavailable)))
        assertFalse(view.text.any { it in '0'..'9' })
    }

    private fun empty(scenario: ActivityScenario<WalletDisplayFixtureActivity>) {
        scenario.onActivity {
            assertTrue(it.window.attributes.flags and WindowManager.LayoutParams.FLAG_SECURE != 0)
            unavailable(it.findViewById(R.id.address_balance))
        }
    }

    private fun begin(scenario: ActivityScenario<WalletDisplayFixtureActivity>): ReadOnlySync.Attempt {
        var attempt: ReadOnlySync.Attempt? = null
        scenario.onActivity { attempt = it.beginFixture() }
        return checkNotNull(attempt)
    }

    private fun complete(scenario: ActivityScenario<WalletDisplayFixtureActivity>): BalanceView {
        val attempt = begin(scenario)
        for (step in 1..6) {
            attempt.request()
            val frame = instrumentation.context.assets.open("sync/mainnet-$step.json").use { it.readBytes() }
            assertEquals(CoreStatus.OK, attempt.reply(frame))
        }
        scenario.onActivity { assertTrue(it.redrawFixture()) }
        instrumentation.waitForIdleSync()
        var view: BalanceView? = null
        scenario.onActivity {
            val current = it.findViewById<BalanceView>(R.id.address_balance)
            assertTrue(current.text.startsWith(it.getString(R.string.balance_unverified)))
            assertTrue(current.text.contains("0.00000993 ZCL"))
            view = current
        }
        return checkNotNull(view)
    }

    private fun cancelled(attempt: ReadOnlySync.Attempt) {
        assertEquals(CoreStatus.CANCELLED, attempt.reply(byteArrayOf()))
        assertEquals(CoreStatus.CANCELLED,
            assertThrows(ReadOnlySyncFailure::class.java) { attempt.request() }.status)
    }

    @Test fun recreationClearsViewsAndReleasesMoreThanTheFourNativeSlots() {
        ActivityScenario.launch(WalletDisplayFixtureActivity::class.java).use { scenario ->
            repeat(5) { // One more than the registry capacity exposes any per-recreation leak.
                empty(scenario)
                val oldView = complete(scenario)
                val lateAttempt = begin(scenario)
                scenario.recreate()
                empty(scenario)
                instrumentation.runOnMainSync { unavailable(oldView) }
                cancelled(lateAttempt)
            }
        }
    }

    @Test fun backgroundClosesLiveAttemptAndResumeRequiresANewCompleteReport() {
        ActivityScenario.launch(WalletDisplayFixtureActivity::class.java).use { scenario ->
            empty(scenario)
            val oldView = complete(scenario)
            val lateAttempt = begin(scenario)
            scenario.moveToState(Lifecycle.State.CREATED)
            instrumentation.runOnMainSync { unavailable(oldView) }
            cancelled(lateAttempt)
            scenario.moveToState(Lifecycle.State.RESUMED)
            empty(scenario)
            complete(scenario)
        }
    }

    @Test fun aNewActivityStartsWithoutAReportOrAutomaticReplay() {
        ActivityScenario.launch(WalletDisplayFixtureActivity::class.java).use { scenario ->
            empty(scenario)
            complete(scenario)
        }
        ActivityScenario.launch(WalletDisplayFixtureActivity::class.java).use { scenario ->
            instrumentation.waitForIdleSync()
            empty(scenario)
        }
    }
}
