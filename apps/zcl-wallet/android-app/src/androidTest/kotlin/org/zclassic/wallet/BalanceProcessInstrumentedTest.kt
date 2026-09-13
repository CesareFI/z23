// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.os.Build
import android.os.Bundle
import android.os.Process
import androidx.test.core.app.ActivityScenario
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertTrue
import org.junit.Assume.assumeTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.zclassic.wallet.core.CoreStatus
import org.zclassic.wallet.core.ReadOnlySync

/** Two-phase opt-in emulator fixture, driven by check-process-relaunch.sh.
 * Preparation is deliberately terminated after reporting verified readiness.
 * No wallet, storage, authentication, key, endpoint or secret participates.
 */
@RunWith(AndroidJUnit4::class)
class BalanceProcessInstrumentedTest {
    private val instrumentation = InstrumentationRegistry.getInstrumentation()

    @Before fun optedInEmulatorOnly() {
        assumeTrue("Requires the bounded process-relaunch controller",
            InstrumentationRegistry.getArguments().getString("processKillFixture") == "yes")
        assertEquals("ranchu", Build.HARDWARE)
        assertEquals("org.zclassic.wallet.dev", instrumentation.targetContext.packageName)
    }

    @Test fun preparePublicReportForTermination() {
        ActivityScenario.launch(WalletDisplayFixtureActivity::class.java).use { scenario ->
            var attempt: ReadOnlySync.Attempt? = null
            scenario.onActivity { attempt = it.beginFixture() }
            val current = checkNotNull(attempt)
            for (step in 1..6) {
                current.request()
                val frame = instrumentation.context.assets.open("sync/mainnet-$step.json").use { it.readBytes() }
                assertEquals(CoreStatus.OK, current.reply(frame))
            }
            scenario.onActivity { assertTrue(it.redrawFixture()) }
            instrumentation.waitForIdleSync()
            scenario.onActivity {
                val view = it.findViewById<BalanceView>(R.id.address_balance)
                assertTrue(view.text.startsWith(it.getString(R.string.balance_unverified)))
                assertTrue(view.text.contains("0.00000993 ZCL"))
            }
            instrumentation.sendStatus(2, Bundle().apply {
                putString("zcl_process_fixture", "ready")
                putInt("zcl_fixture_pid", Process.myPid())
            })
            // The external controller must kill this process while state is live.
            CountDownLatch(1).await(90, TimeUnit.SECONDS)
            throw AssertionError("Controller did not terminate the prepared fixture process")
        }
    }

    @Test fun newProcessStartsWithoutBalanceOrReplay() {
        val previousPid = checkNotNull(InstrumentationRegistry.getArguments().getString("previousPid")?.toIntOrNull())
        assertTrue(previousPid > 0)
        assertNotEquals(previousPid, Process.myPid())
        ActivityScenario.launch(WalletDisplayFixtureActivity::class.java).use { scenario ->
            instrumentation.waitForIdleSync()
            scenario.onActivity {
                val view = it.findViewById<BalanceView>(R.id.address_balance)
                assertTrue(view.text.startsWith(it.getString(R.string.balance_unavailable)))
                assertFalse(view.text.any { character -> character in '0'..'9' })
                val attempt = it.beginFixture()
                assertTrue(attempt.request().toString(Charsets.US_ASCII).contains("server.version"))
                assertEquals(CoreStatus.CANCELLED, attempt.fail())
            }
            instrumentation.sendStatus(2, Bundle().apply { putInt("zcl_relaunch_pid", Process.myPid()) })
        }
    }
}
