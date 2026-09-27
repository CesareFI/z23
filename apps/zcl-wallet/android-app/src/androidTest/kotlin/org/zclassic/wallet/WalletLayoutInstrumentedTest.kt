// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.app.Activity
import android.graphics.Rect
import android.view.View
import android.view.ViewGroup
import android.view.WindowInsets
import androidx.test.core.app.ActivityScenario
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith
import org.zclassic.wallet.core.Network
import org.zclassic.wallet.core.TransparentAddress

/** Public controls on a storage-free debug host. Real system-bar insets and
 * a constrained viewport must leave every requested action fully reachable. */
@RunWith(AndroidJUnit4::class)
class WalletLayoutInstrumentedTest {
    private fun compact(activity: Activity): Pair<ViewGroup, Rect> {
        val root = activity.findViewById<ViewGroup>(android.R.id.content).getChildAt(0) as ViewGroup
        val insets = checkNotNull(activity.window.decorView.rootWindowInsets)
        val bars = insets.getInsets(WindowInsets.Type.systemBars() or WindowInsets.Type.displayCutout())
        assertTrue("Fixture needs nonzero system-bar insets", bars.top + bars.bottom > 0)
        root.dispatchApplyWindowInsets(insets)
        val density = activity.resources.displayMetrics.density
        val width = (320 * density).toInt()
        val height = (240 * density).toInt()
        root.measure(View.MeasureSpec.makeMeasureSpec(width, View.MeasureSpec.EXACTLY),
            View.MeasureSpec.makeMeasureSpec(height, View.MeasureSpec.EXACTLY))
        root.layout(0, 0, width, height)
        return root to Rect(bars.left, bars.top, width - bars.right, height - bars.bottom)
    }

    private fun reveal(root: ViewGroup, viewport: Rect, view: View) {
        val whole = Rect(0, 0, view.width, view.height)
        assertTrue("Wallet control has no area: ${view.id}", whole.width() > 0 && whole.height() > 0)
        view.requestRectangleOnScreen(Rect(whole), true)
        val visible = Rect()
        assertTrue("Wallet control is unreachable: ${view.id}", view.getLocalVisibleRect(visible))
        assertEquals("Wallet control is clipped: ${view.id}", whole, visible)
        val origin = IntArray(2)
        val target = IntArray(2)
        root.getLocationInWindow(origin)
        view.getLocationInWindow(target)
        val position = Rect(whole).apply { offset(target[0] - origin[0], target[1] - origin[1]) }
        assertTrue("Wallet control ${view.id} at $position overlaps system bars outside $viewport",
            viewport.contains(position))
    }

    private fun withScreen(draw: (WalletScreens) -> Unit, check: (Activity, ViewGroup, Rect) -> Unit) {
        ActivityScenario.launch(WalletDisplayFixtureActivity::class.java).use { scenario ->
            scenario.onActivity { activity ->
                val screens = WalletScreens(activity)
                try {
                    draw(screens)
                    val (root, viewport) = compact(activity)
                    check(activity, root, viewport)
                } finally { screens.clearSecrets() }
            }
        }
    }

    @Test fun compactWelcomeKeepsNetworkAndAllActionsClearOfSystemBars() {
        var created: Network? = null
        var restored: Network? = null
        var scanned: Network? = null
        withScreen({ it.welcome({ network -> created = network }, { network -> restored = network },
            { network -> scanned = network }) }) { activity, root, viewport ->
            val mainnet = activity.findViewById<View>(R.id.network_mainnet)
            reveal(root, viewport, mainnet)
            mainnet.performClick()
            for (identifier in intArrayOf(R.id.create_wallet, R.id.restore_wallet, R.id.scan_request)) {
                val control = activity.findViewById<View>(identifier)
                reveal(root, viewport, control)
                assertTrue(control.performClick())
            }
            assertEquals(Network.MAINNET, created)
            assertEquals(Network.MAINNET, restored)
            assertEquals(Network.MAINNET, scanned)
        }
    }

    @Test fun compactReceiveKeepsPublicAddressAndActionsClearOfSystemBars() {
        val address = TransparentAddress.fromPublicKeyHash(ByteArray(20), Network.MAINNET)
        var locked = false
        var scanned = false
        withScreen({ it.receive(address, { locked = true }, { scanned = true }) }) { activity, root, viewport ->
            reveal(root, viewport, activity.findViewById(R.id.receiving_address))
            for (identifier in intArrayOf(R.id.lock_wallet, R.id.scan_request)) {
                val control = activity.findViewById<View>(identifier)
                reveal(root, viewport, control)
                assertTrue(control.performClick())
            }
            assertTrue(locked)
            assertTrue(scanned)
        }
    }

    @Test fun compactBackupKeepsConfirmationAndCancelClearOfSystemBars() {
        val marker = charArrayOf('a', 'b', 'c')
        var confirmed = false
        var cancelled = false
        try {
            withScreen({ it.backup(marker, { confirmed = true }, { cancelled = true }) }) { activity, root, viewport ->
                for (identifier in intArrayOf(R.id.backup_written, R.id.cancel_setup)) {
                    val control = activity.findViewById<View>(identifier)
                    reveal(root, viewport, control)
                    assertTrue(control.performClick())
                }
                assertTrue(confirmed)
                assertTrue(cancelled)
            }
            assertTrue(marker.all { it == '\u0000' })
        } finally { marker.fill('\u0000') }
    }

    @Test fun compactRecoveryKeepsSubmitAndCancelClearOfSystemBars() {
        var submitted = false
        var cancelled = false
        withScreen({ it.enterRecovery(false, false, { owned -> owned.fill('\u0000'); submitted = true },
            { cancelled = true }) }) { activity, root, viewport ->
            for (identifier in intArrayOf(R.id.save_wallet, R.id.cancel_setup)) {
                val control = activity.findViewById<View>(identifier)
                reveal(root, viewport, control)
                assertTrue(control.performClick())
            }
            assertTrue(submitted)
            assertTrue(cancelled)
        }
    }
}
