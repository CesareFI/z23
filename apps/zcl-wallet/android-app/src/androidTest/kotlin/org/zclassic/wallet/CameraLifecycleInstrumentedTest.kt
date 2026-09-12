// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.Manifest
import android.content.pm.PackageManager
import android.os.Build
import android.os.SystemClock
import android.view.View
import android.view.WindowManager
import androidx.lifecycle.Lifecycle
import androidx.test.core.app.ActivityScenario
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicBoolean
import org.junit.Assert.*
import org.junit.Assume.assumeTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith

/** Explicit emulator-only camera acceptance. It never opens or removes a wallet. */
@RunWith(AndroidJUnit4::class)
class CameraLifecycleInstrumentedTest {
    private val instrumentation = InstrumentationRegistry.getInstrumentation()

    @Before fun emulatorOnly() {
        assumeTrue("Requires camera emulator fixture opt-in",
            InstrumentationRegistry.getArguments().getString("cameraFixture") == "yes")
        assertEquals("ranchu", Build.HARDWARE)
        assertEquals("org.zclassic.wallet.dev", instrumentation.targetContext.packageName)
    }

    private fun await(message: String, condition: () -> Boolean) {
        val deadline = SystemClock.uptimeMillis() + 45_000
        while (SystemClock.uptimeMillis() < deadline) {
            if (condition()) return
            SystemClock.sleep(50)
        }
        throw AssertionError(message)
    }

    private fun cameraThreadsClosed() = Thread.getAllStackTraces().keys.none { it.name == "WalletCamera" && it.isAlive }

    @Test fun deniedPermissionClosesCameraWorkerWithoutFrame() {
        assertEquals(PackageManager.PERMISSION_DENIED,
            instrumentation.targetContext.checkSelfPermission(Manifest.permission.CAMERA))
        val failed = CountDownLatch(1)
        val gotFrame = AtomicBoolean()
        val camera = CameraCapture(instrumentation.targetContext,
            { owner, _, _, _ -> gotFrame.set(true); owner.frameDone() }, { failed.countDown() })
        try {
            instrumentation.runOnMainSync { camera.start() }
            assertTrue(failed.await(20, TimeUnit.SECONDS))
            await("Denied camera left a worker alive", ::cameraThreadsClosed)
            assertFalse(gotFrame.get())
        } finally { instrumentation.runOnMainSync { camera.close() } }
    }

    @Test fun actualFramesStopOnBackgroundAndRequireExplicitRestart() {
        assertEquals(PackageManager.PERMISSION_GRANTED,
            instrumentation.targetContext.checkSelfPermission(Manifest.permission.CAMERA))
        ActivityScenario.launch(CameraScanActivity::class.java).use { scenario ->
            repeat(3) {
                scenario.onActivity { activity ->
                    assertTrue(activity.window.attributes.flags and WindowManager.LayoutParams.FLAG_SECURE != 0)
                    assertNotNull(activity.findViewById<View>(R.id.scan_start))
                    assertNull(activity.findViewById<View>(R.id.scan_preview))
                    assertTrue(activity.findViewById<View>(R.id.scan_start).performClick())
                }
                var preview: CameraPreviewView? = null
                await("Camera did not deliver a sampled frame") {
                    var ready = false
                    scenario.onActivity { activity ->
                        preview = activity.findViewById(R.id.scan_preview)
                        ready = preview?.hasFrame == true
                    }
                    ready
                }
                scenario.moveToState(Lifecycle.State.CREATED)
                instrumentation.runOnMainSync { assertFalse(requireNotNull(preview).hasFrame) }
                await("Background capture left its worker alive", ::cameraThreadsClosed)
                scenario.moveToState(Lifecycle.State.RESUMED)
                scenario.onActivity { activity ->
                    assertNotNull(activity.findViewById<View>(R.id.scan_start))
                    assertNull(activity.findViewById<View>(R.id.scan_preview))
                }
                assertTrue(cameraThreadsClosed())
            }
        }
        await("Destroyed scanner left its worker alive", ::cameraThreadsClosed)
    }
}
