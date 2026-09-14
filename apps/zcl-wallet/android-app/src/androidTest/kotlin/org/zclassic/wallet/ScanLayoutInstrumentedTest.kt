// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

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
import org.junit.Test
import org.junit.runner.RunWith
import org.zclassic.wallet.core.Network

/** Public layout only: a constrained window must retain reachable actions.
 * This neither changes device configuration nor opens camera/wallet storage. */
@RunWith(AndroidJUnit4::class)
class ScanLayoutInstrumentedTest {
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

    @Test fun compactChooserKeepsNetworkStartAndCloseReachable() {
        val instrumentation = InstrumentationRegistry.getInstrumentation()
        assumeTrue("Requires public scanner fixture opt-in",
            InstrumentationRegistry.getArguments().getString("cameraFixture") == "yes")
        assertEquals("ranchu", Build.HARDWARE)
        assertEquals("org.zclassic.wallet.dev", instrumentation.targetContext.packageName)
        ActivityScenario.launch(CameraScanActivity::class.java).use { scenario ->
            scenario.onActivity { activity ->
                var selected = Network.TESTNET
                var started: Network? = null
                var closed = false
                val screens = ScanScreens(activity)
                screens.choose(Network.TESTNET, R.string.scan_description,
                    { selected = it }, { started = it }, { closed = true })
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
                val mainnet = activity.findViewById<View>(R.id.network_mainnet)
                reveal(root, mainnet)
                // RadioButton toggles through checked-state listeners; its
                // performClick return only reports a separate click listener.
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
                screens.clear()
            }
        }
    }
}
