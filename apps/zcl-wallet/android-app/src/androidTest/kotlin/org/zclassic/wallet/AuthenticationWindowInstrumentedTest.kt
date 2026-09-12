// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.os.SystemClock
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.zclassic.wallet.core.WrappingPolicy

/** Public timestamps through the actual device JNI library. No prompt, key,
 * wallet or device-clock mutation. These check the native time gate, not
 * successful hardware authentication or real device suspend/resume. */
@RunWith(AndroidJUnit4::class)
class AuthenticationWindowInstrumentedTest {
    @Test fun elapsedDeadlineRefusesSimulatedSleepAndDelayedDelivery() {
        val start = SystemClock.elapsedRealtime()
        assertEquals(90_000L, WrappingPolicy.authenticationWindowMillis)
        assertTrue(WrappingPolicy.authenticationWindowOpen(start, start))
        assertTrue(WrappingPolicy.authenticationWindowOpen(start, start + 89_999))
        assertFalse(WrappingPolicy.authenticationWindowOpen(start, start + 90_000))
        assertFalse(WrappingPolicy.authenticationWindowOpen(start, start + 3_600_000))
        assertFalse(WrappingPolicy.authenticationWindowOpen(start, start - 1))
    }

    @Test fun signedJniTimeBoundsCannotReopenExpiredAuthentication() {
        assertFalse(WrappingPolicy.authenticationWindowOpen(-1, 0))
        assertFalse(WrappingPolicy.authenticationWindowOpen(0, Long.MIN_VALUE))
        assertFalse(WrappingPolicy.authenticationWindowOpen(0, Long.MAX_VALUE))
        assertTrue(WrappingPolicy.authenticationWindowOpen(Long.MAX_VALUE - 1, Long.MAX_VALUE))
    }
}
