// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.Manifest
import android.accessibilityservice.AccessibilityServiceInfo
import android.annotation.SuppressLint
import android.content.ComponentName
import android.content.Intent
import android.content.pm.ApplicationInfo
import android.content.pm.PackageManager
import android.graphics.Rect
import android.os.Build
import android.os.SystemClock
import android.view.InputDevice
import android.view.MotionEvent
import android.view.accessibility.AccessibilityNodeInfo
import androidx.test.platform.app.InstrumentationRegistry
import java.io.File
import org.junit.Assert.*
import org.junit.Assume.assumeTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.junit.runners.JUnit4

/** Public UI only: no references to app classes or numeric resource IDs, so
 * the same test APK can exercise a locally signed minified build. */
@RunWith(JUnit4::class)
class ScannerPermissionInstrumentedTest {
    private val instrumentation = InstrumentationRegistry.getInstrumentation()
    private val context = instrumentation.targetContext
    private val ui get() = instrumentation.uiAutomation

    @Before fun emptyEmulatorOnly() {
        val arguments = InstrumentationRegistry.getArguments()
        assumeTrue("Requires a fresh disposable permission fixture",
            arguments.getString("scannerPermissionFixture") == "yes")
        assertEquals("ranchu", Build.HARDWARE)
        assertEquals("org.zclassic.wallet.dev", context.packageName)
        assertFalse("Existing wallet directory is outside this fixture",
            File(context.noBackupFilesDir, "wallet-v1").exists())
        assertEquals(PackageManager.PERMISSION_DENIED,
            context.checkSelfPermission(Manifest.permission.CAMERA))
        if (arguments.getString("requireMinified") == "yes")
            assertEquals(0, context.applicationInfo.flags and ApplicationInfo.FLAG_DEBUGGABLE)
        val info = ui.serviceInfo
        info.flags = info.flags or AccessibilityServiceInfo.FLAG_REPORT_VIEW_IDS
        ui.serviceInfo = info
    }

    private fun await(message: String, condition: () -> Boolean) {
        val started = SystemClock.uptimeMillis()
        while (SystemClock.uptimeMillis() - started < 45_000) {
            if (condition()) return
            SystemClock.sleep(100)
        }
        throw AssertionError("$message; ${foreground()}")
    }

    @Suppress("DEPRECATION") // Report only window metadata, never screen text.
    private fun foreground(): String {
        val root = ui.rootInActiveWindow ?: return "no accessibility window"
        try { return "window=${root.packageName}/${root.className}, children=${root.childCount}" }
        finally { root.recycle() }
    }

    @Suppress("DEPRECATION") // Release the API30..32 node pool as well.
    private fun node(identifier: String, inspect: (AccessibilityNodeInfo) -> Boolean): Boolean {
        val root = ui.rootInActiveWindow ?: return false
        try {
            val nodes = root.findAccessibilityNodeInfosByViewId(identifier)
            try { return nodes.size == 1 && inspect(nodes.single()) }
            finally { nodes.forEach { it.recycle() } }
        } finally { root.recycle() }
    }

    private fun appId(name: String) = "${context.packageName}:id/$name"

    @Suppress("DEPRECATION") // The boolean getter is required on API30..35.
    private fun checked(node: AccessibilityNodeInfo): Boolean =
        if (Build.VERSION.SDK_INT >= 36) node.checked == AccessibilityNodeInfo.CHECKED_STATE_TRUE
        else node.isChecked

    private fun click(identifier: String) {
        val bounds = Rect()
        await("Missing clickable fixture control: $identifier") {
            node(identifier) {
                it.getBoundsInScreen(bounds)
                it.isEnabled && it.isClickable && !bounds.isEmpty
            }
        }
        // Permission controls appear before their entrance animation settles.
        // Wait for platform UI idleness, then obtain the current touch bounds.
        ui.waitForIdle(1_000, 10_000)
        assertTrue(node(identifier) {
            it.getBoundsInScreen(bounds)
            it.isEnabled && it.isClickable && !bounds.isEmpty
        })
        val started = SystemClock.uptimeMillis()
        val down = MotionEvent.obtain(started, started, MotionEvent.ACTION_DOWN,
            bounds.centerX().toFloat(), bounds.centerY().toFloat(), 0)
        down.source = InputDevice.SOURCE_TOUCHSCREEN
        try {
            assertTrue(ui.injectInputEvent(down, true))
            val up = MotionEvent.obtain(started, SystemClock.uptimeMillis(), MotionEvent.ACTION_UP,
                bounds.centerX().toFloat(), bounds.centerY().toFloat(), 0)
            up.source = InputDevice.SOURCE_TOUCHSCREEN
            try { assertTrue(ui.injectInputEvent(up, true)) }
            finally { up.recycle() }
        } finally { down.recycle() }
    }

    @SuppressLint("DiscouragedApi") // IDs differ between debug and minified APKs.
    private fun publicString(name: String): String {
        val identifier = context.resources.getIdentifier(name, "string", context.packageName)
        assertNotEquals(0, identifier)
        return context.getString(identifier)
    }

    @Test fun actualPermissionDenialRetainsItsExplanationAndSelectedNetwork() {
        val launch = Intent().setComponent(ComponentName(context.packageName,
            "org.zclassic.wallet.CameraScanActivity")).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
        val activity = instrumentation.startActivitySync(launch)
        try {
            click(appId("network_mainnet"))
            click(appId("scan_start"))
            click("com.android.permissioncontroller:id/permission_deny_button")
            await("Android did not close its permission dialog after denial") {
                node(appId("scan_start")) { it.isEnabled }
            }
            val expected = publicString("scan_permission_denied")
            await("Permission denial explanation was lost on Activity resume") {
                node(appId("status_message")) { it.text?.toString() == expected }
            }
            assertTrue(node(appId("network_mainnet"), ::checked))
            assertTrue(node(appId("scan_start")) { it.isEnabled })
            assertFalse(node(appId("scan_preview")) { true })
            assertFalse(node(appId("scan_address")) { true })
            assertEquals(PackageManager.PERMISSION_DENIED,
                context.checkSelfPermission(Manifest.permission.CAMERA))
            assertTrue(Thread.getAllStackTraces().keys.none { it.name == "WalletCamera" && it.isAlive })
            assertFalse(File(context.noBackupFilesDir, "wallet-v1").exists())
            click(appId("scan_close"))
        } finally { instrumentation.runOnMainSync { activity.finish() } }
    }
}
