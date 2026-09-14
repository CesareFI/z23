// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.app.Activity
import android.graphics.Rect
import android.os.Build
import android.os.Bundle
import android.view.View
import android.view.ViewGroup
import androidx.test.core.app.ActivityScenario
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import org.junit.Assert.*
import org.junit.Assume.assumeTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.zclassic.wallet.core.Network
import org.zclassic.wallet.core.PaymentRequest

/** Public layout only: a constrained window must retain reachable actions.
 * This neither changes device configuration nor opens camera/wallet storage. */
@RunWith(AndroidJUnit4::class)
class ScanLayoutInstrumentedTest {
    private val instrumentation = InstrumentationRegistry.getInstrumentation()

    @Before fun emulatorOnly() {
        assumeTrue("Requires public scanner fixture opt-in",
            InstrumentationRegistry.getArguments().getString("cameraFixture") == "yes")
        assertEquals("ranchu", Build.HARDWARE)
        assertEquals("org.zclassic.wallet.dev", instrumentation.targetContext.packageName)
    }

    private fun reveal(root: ViewGroup, view: View) {
        val whole = Rect(0, 0, view.width, view.height)
        // Android translates the supplied rectangle while walking ancestors.
        view.requestRectangleOnScreen(Rect(whole), true)
        val visible = Rect()
        assertTrue("Scanner control is unreachable: ${view.id}", view.getLocalVisibleRect(visible))
        assertEquals("Scanner control is clipped: ${view.id}", whole, visible)
        val position = Rect(whole)
        root.offsetDescendantRectToMyCoords(view, position)
        val viewport = Rect(root.paddingLeft, root.paddingTop,
            root.width - root.paddingRight, root.height - root.paddingBottom)
        assertTrue("Scanner control ${view.id} at $position escapes viewport $viewport",
            viewport.contains(position))
    }

    private fun compact(activity: Activity): ViewGroup {
        val content = activity.findViewById<ViewGroup>(android.R.id.content)
        val root = content.getChildAt(0) as ViewGroup
        root.dispatchApplyWindowInsets(checkNotNull(activity.window.decorView.rootWindowInsets))
        val density = activity.resources.displayMetrics.density
        val width = (320 * density).toInt()
        val height = (240 * density).toInt()
        root.measure(View.MeasureSpec.makeMeasureSpec(width, View.MeasureSpec.EXACTLY),
            View.MeasureSpec.makeMeasureSpec(height, View.MeasureSpec.EXACTLY))
        root.layout(0, 0, width, height)
        instrumentation.sendStatus(2, Bundle().apply {
            putString("scanner_viewport", "${root.width}x${root.height}")
            putString("scanner_padding", "${root.paddingLeft},${root.paddingTop},${root.paddingRight},${root.paddingBottom}")
        })
        return root
    }

    private fun withScreen(draw: (ScanScreens) -> Unit, check: (Activity, ViewGroup) -> Unit) {
        ActivityScenario.launch(CameraScanActivity::class.java).use { scenario ->
            scenario.onActivity { activity ->
                val screens = ScanScreens(activity)
                try { draw(screens); check(activity, compact(activity)) }
                finally { screens.clear() }
            }
        }
    }

    @Test fun compactChooserKeepsNetworkStartAndCloseReachable() {
        var selected = Network.TESTNET
        var started: Network? = null
        var closed = false
        withScreen({ it.choose(Network.TESTNET, R.string.scan_description,
            { selected = it }, { started = it }, { closed = true }) }) { activity, root ->
            val mainnet = activity.findViewById<View>(R.id.network_mainnet)
            reveal(root, mainnet)
            // RadioButton's return reports a click listener, not checked state.
            mainnet.performClick()
            assertEquals(Network.MAINNET, selected)
            val start = activity.findViewById<View>(R.id.scan_start)
            reveal(root, start)
            assertTrue(start.performClick())
            assertEquals(Network.MAINNET, started)
            val close = activity.findViewById<View>(R.id.scan_close)
            reveal(root, close)
            assertTrue(close.performClick())
            assertTrue(closed)
        }
    }

    @Test fun compactCaptureKeepsCancelReachable() {
        var cancelled = false
        withScreen({ it.scanning(Network.MAINNET) { cancelled = true } }) { activity, root ->
            val cancel = activity.findViewById<View>(R.id.scan_cancel)
            reveal(root, cancel)
            assertTrue(cancel.performClick())
            assertTrue(cancelled)
        }
    }

    @Test fun compactReviewKeepsPublicRequestAndActionsReachable() {
        val request = PaymentRequest.parse(
            "zclassic:t1T8yaLVhNqxA5KJcmiqqFN88e8DNp2PBfF?amount=1.25&label=CameraFixture", Network.MAINNET)
        var again = false
        var closed = false
        withScreen({ it.review(request, { again = true }, { closed = true }) }) { activity, root ->
            for (identifier in intArrayOf(R.id.scan_address, R.id.scan_amount, R.id.scan_label))
                reveal(root, activity.findViewById(identifier))
            val start = activity.findViewById<View>(R.id.scan_start)
            reveal(root, start)
            assertTrue(start.performClick())
            assertTrue(again)
            val close = activity.findViewById<View>(R.id.scan_close)
            reveal(root, close)
            assertTrue(close.performClick())
            assertTrue(closed)
        }
    }
}
