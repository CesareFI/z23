// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.os.Build
import android.os.Bundle
import android.os.Process
import android.os.SystemClock
import androidx.test.core.app.ActivityScenario
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import org.junit.Assert.*
import org.junit.Assume.assumeTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.zclassic.wallet.core.Network
import org.zclassic.wallet.core.UnsignedReview
import org.zclassic.wallet.core.Zatoshi

/** Two-phase opt-in public process fixture. Only the exact prepared emulator
 * process may be terminated by check-process-relaunch.sh after readiness.
 */
@RunWith(AndroidJUnit4::class)
class ReviewProcessInstrumentedTest {
    private val instrumentation = InstrumentationRegistry.getInstrumentation()
    private val transactionId = "602c673db0503b48a400414347009ae968b1aaba9b10663bde508e7f8218dc46"
    private fun fixture(name: String) = instrumentation.context.assets.open("review/$name").use { it.readBytes() }
    private fun owner() = UnsignedReview.open(fixture("draft"),
        arrayOf(fixture("previous0"), fixture("previous1")), Network.MAINNET,
        Zatoshi.of(500), SystemClock::elapsedRealtime)

    @Before fun optedInEmulatorOnly() {
        assumeTrue("Requires the bounded process-relaunch controller",
            InstrumentationRegistry.getArguments().getString("processKillFixture") == "yes")
        assertEquals("review", InstrumentationRegistry.getArguments().getString("reportProfile"))
        assertEquals("ranchu", Build.HARDWARE)
        assertEquals("org.zclassic.wallet.dev", instrumentation.targetContext.packageName)
    }

    @Test fun preparePublicReportForTermination() {
        ActivityScenario.launch(WalletReviewFixtureActivity::class.java).use { scenario ->
            owner().use { review ->
                scenario.onActivity { assertTrue(it.showFixture(review)) }
                instrumentation.waitForIdleSync()
                scenario.onActivity {
                    val view = it.findViewById<ReviewView>(R.id.transaction_review)
                    assertTrue(view.text.startsWith(it.getString(R.string.review_unsigned)))
                    assertTrue(view.text.contains(transactionId))
                    assertTrue(view.text.contains("Fee: 0.000005 ZCL"))
                    assertEquals(transactionId, review.snapshot().transactionId)
                }
                instrumentation.sendStatus(2, Bundle().apply {
                    putString("zcl_process_fixture", "ready")
                    putInt("zcl_fixture_pid", Process.myPid())
                    putString("zcl_fixture_profile", "review")
                })
                CountDownLatch(1).await(90, TimeUnit.SECONDS)
                throw AssertionError("Controller did not terminate the prepared review process")
            }
        }
    }

    @Test fun newProcessStartsWithoutReviewOrReplay() {
        val previousPid = checkNotNull(InstrumentationRegistry.getArguments().getString("previousPid")?.toIntOrNull())
        assertTrue(previousPid > 0)
        assertNotEquals(previousPid, Process.myPid())
        ActivityScenario.launch(WalletReviewFixtureActivity::class.java).use { scenario ->
            instrumentation.waitForIdleSync()
            scenario.onActivity {
                val view = it.findViewById<ReviewView>(R.id.transaction_review)
                assertTrue(view.text.startsWith(it.getString(R.string.review_unavailable)))
                assertFalse(view.text.any { character -> character in '0'..'9' })
                assertFalse(view.text.contains(transactionId))
            }
            // Restart has no active draft occupying the native slot. Preparing
            // a new public fixture does not automatically publish it to the view.
            owner().use { assertEquals(transactionId, it.snapshot().transactionId) }
            scenario.onActivity {
                val view = it.findViewById<ReviewView>(R.id.transaction_review)
                assertFalse(view.text.any { character -> character in '0'..'9' })
            }
            instrumentation.sendStatus(2, Bundle().apply { putInt("zcl_relaunch_pid", Process.myPid()) })
        }
    }
}
