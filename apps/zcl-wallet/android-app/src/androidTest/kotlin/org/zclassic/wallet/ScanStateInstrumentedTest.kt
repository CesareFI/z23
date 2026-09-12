// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.content.Intent
import android.view.View
import android.widget.RadioButton
import androidx.lifecycle.Lifecycle
import androidx.test.core.app.ActivityScenario
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

/** Public network preference only. Never starts a camera, opens a wallet,
 * requests permission, captures an image or persists request/secret content. */
@RunWith(AndroidJUnit4::class)
class ScanStateInstrumentedTest {
    private fun selected(scenario: ActivityScenario<CameraScanActivity>, identifier: Int) {
        scenario.onActivity { activity ->
            assertTrue(activity.findViewById<RadioButton>(identifier).isChecked)
            assertNotNull(activity.findViewById<View>(R.id.scan_start))
            assertNull(activity.findViewById<View>(R.id.scan_preview))
            assertNull(activity.findViewById<View>(R.id.scan_address))
        }
    }

    @Test fun explicitNetworkChoiceSurvivesRecreationAndBackgroundWithoutStartingCapture() {
        val context = InstrumentationRegistry.getInstrumentation().targetContext
        val launch = Intent(context, CameraScanActivity::class.java).putExtra("mainnet", true)
        ActivityScenario.launch<CameraScanActivity>(launch).use { scenario ->
            selected(scenario, R.id.network_mainnet)
            scenario.onActivity { activity ->
                activity.findViewById<RadioButton>(R.id.network_testnet).performClick()
            }
            selected(scenario, R.id.network_testnet)
            scenario.recreate()
            selected(scenario, R.id.network_testnet)
            scenario.onActivity { activity ->
                activity.findViewById<RadioButton>(R.id.network_mainnet).performClick()
            }
            scenario.moveToState(Lifecycle.State.CREATED)
            scenario.moveToState(Lifecycle.State.RESUMED)
            selected(scenario, R.id.network_mainnet)
            scenario.recreate()
            selected(scenario, R.id.network_mainnet)
        }
    }
}
