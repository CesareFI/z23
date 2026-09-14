// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.Manifest
import android.content.pm.PackageManager
import android.os.Build
import android.os.SystemClock
import android.view.View
import android.widget.RadioButton
import android.widget.TextView
import androidx.lifecycle.Lifecycle
import androidx.test.core.app.ActivityScenario
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import org.junit.Assert.*
import org.junit.Assume.assumeTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith

/** A disposable emulator with the documented public QR camera image. Uses the
 * actual camera, isolated decoder and Activity; never opens a wallet. */
@RunWith(AndroidJUnit4::class)
class CameraRequestInstrumentedTest {
    private val instrumentation = InstrumentationRegistry.getInstrumentation()
    private val address = "t1T8yaLVhNqxA5KJcmiqqFN88e8DNp2PBfF"

    @Before fun publicSceneOnly() {
        assumeTrue("Requires the public QR emulator camera scene",
            InstrumentationRegistry.getArguments().getString("qrCameraFixture") == "yes")
        assertEquals("ranchu", Build.HARDWARE)
        assertEquals("org.zclassic.wallet.dev", instrumentation.targetContext.packageName)
        assertEquals(PackageManager.PERMISSION_GRANTED,
            instrumentation.targetContext.checkSelfPermission(Manifest.permission.CAMERA))
    }

    private fun await(message: String, condition: () -> Boolean) {
        val started = SystemClock.uptimeMillis()
        while (SystemClock.uptimeMillis() - started < 60_000) {
            if (condition()) return
            SystemClock.sleep(50)
        }
        throw AssertionError(message)
    }

    private fun cameraClosed() = Thread.getAllStackTraces().keys.none { it.name == "WalletCamera" && it.isAlive }

    private fun scanAndReview(scenario: ActivityScenario<CameraScanActivity>) {
        var preview: CameraPreviewView? = null
        scenario.onActivity { activity ->
            assertNull(activity.findViewById<View>(R.id.scan_address))
            activity.findViewById<RadioButton>(R.id.network_mainnet).isChecked = true
            assertTrue(activity.findViewById<View>(R.id.scan_start).performClick())
            preview = activity.findViewById(R.id.scan_preview)
            assertNotNull(preview)
        }
        await("Camera QR did not reach public request review") {
            var reviewed = false
            scenario.onActivity { activity ->
                assertNull("Camera or isolated decoder returned to retry",
                    activity.findViewById<View>(R.id.network_mainnet))
                reviewed = activity.findViewById<TextView>(R.id.scan_address)?.text?.toString() == address
            }
            reviewed
        }
        scenario.onActivity { activity ->
            assertEquals(activity.getString(R.string.scan_amount, "1.25"),
                activity.findViewById<TextView>(R.id.scan_amount).text.toString())
            assertEquals(activity.getString(R.string.scan_label, "CameraFixture"),
                activity.findViewById<TextView>(R.id.scan_label).text.toString())
            assertEquals(activity.getString(R.string.scan_review_notice),
                activity.findViewById<TextView>(R.id.status_message).text.toString())
            assertEquals(activity.getString(R.string.scan_again),
                activity.findViewById<TextView>(R.id.scan_start).text.toString())
            assertNull(activity.findViewById<View>(R.id.scan_preview))
            assertFalse(checkNotNull(preview).hasFrame)
        }
        await("Review retained its camera worker", ::cameraClosed)
    }

    @Test fun cameraImageReachesExactReviewAndRecreationRequiresRescan() {
        ActivityScenario.launch(CameraScanActivity::class.java).use { scenario ->
            scanAndReview(scenario)
            scenario.recreate()
            scenario.onActivity { activity ->
                assertNull(activity.findViewById<View>(R.id.scan_address))
                assertNull(activity.findViewById<View>(R.id.scan_preview))
                assertNotNull(activity.findViewById<View>(R.id.scan_start))
            }
            assertTrue(cameraClosed())
            scanAndReview(scenario)
            scenario.moveToState(Lifecycle.State.CREATED)
            await("Background scanner retained a camera worker", ::cameraClosed)
        }
        await("Destroyed scanner retained a camera worker", ::cameraClosed)
    }
}
