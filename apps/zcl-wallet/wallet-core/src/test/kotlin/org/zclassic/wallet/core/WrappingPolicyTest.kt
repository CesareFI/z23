// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet.core

import kotlin.test.Test
import kotlin.test.assertFalse
import kotlin.test.assertTrue
import kotlin.test.assertEquals

class WrappingPolicyTest {
    @Test fun setupDelayKeepsItsOriginalElapsedDeadline() {
        for (start in listOf(0L, 1L, 123_456_789L, Long.MAX_VALUE - 600_000)) {
            assertEquals(600_000L, WrappingPolicy.setupWindowRemainingMillis(start, start))
            assertEquals(1L, WrappingPolicy.setupWindowRemainingMillis(start, start + 599_999))
            assertEquals(0L, WrappingPolicy.setupWindowRemainingMillis(start, start + 600_000))
            if (start > 0) assertEquals(0L, WrappingPolicy.setupWindowRemainingMillis(start, start - 1))
        }
        for ((start, now) in listOf(-1L to 0L, 0L to -1L, -1L to -1L, Long.MIN_VALUE to Long.MIN_VALUE,
            Long.MIN_VALUE to Long.MAX_VALUE, 0L to Long.MAX_VALUE)) {
            assertEquals(0L, WrappingPolicy.setupWindowRemainingMillis(start, now))
        }
        assertEquals(600_000L, WrappingPolicy.setupWindowRemainingMillis(Long.MAX_VALUE, Long.MAX_VALUE))
    }

    @Test fun authenticationContinuationUsesBoundedElapsedTimeAcrossJni() {
        assertEquals(90_000L, WrappingPolicy.authenticationWindowMillis)
        for (start in listOf(0L, 1L, 123_456_789L, Long.MAX_VALUE - 90_000)) {
            assertTrue(WrappingPolicy.authenticationWindowOpen(start, start))
            assertTrue(WrappingPolicy.authenticationWindowOpen(start, start + 89_999))
            assertFalse(WrappingPolicy.authenticationWindowOpen(start, start + 90_000))
            if (start > 0) assertFalse(WrappingPolicy.authenticationWindowOpen(start, start - 1))
        }
        assertFalse(WrappingPolicy.authenticationWindowOpen(-1, 0))
        assertFalse(WrappingPolicy.authenticationWindowOpen(0, -1))
        assertFalse(WrappingPolicy.authenticationWindowOpen(Long.MIN_VALUE, Long.MAX_VALUE))
        assertFalse(WrappingPolicy.authenticationWindowOpen(0, Long.MAX_VALUE))
        assertTrue(WrappingPolicy.authenticationWindowOpen(Long.MAX_VALUE, Long.MAX_VALUE))
    }
    @Test fun onlyCompleteHardwarePerUsePolicyIsAccepted() {
        for (hardware in listOf(WrappingPolicy.TEE, WrappingPolicy.STRONGBOX)) {
            assertTrue(WrappingPolicy.accepts(256, hardware, 15, 0, 3))
            assertTrue(WrappingPolicy.accepts(256, hardware, 15, -1, 3))
        }
        assertFalse(WrappingPolicy.accepts(256, 0, 15, 0, 3))
        assertFalse(WrappingPolicy.accepts(128, 1, 15, 0, 3))
        assertFalse(WrappingPolicy.accepts(256, 1, 15, 30, 3))
        assertFalse(WrappingPolicy.accepts(256, 1, 15, 0, 1))
        for (bit in 0..30)
            assertFalse(WrappingPolicy.accepts(256, 1, 15 xor (1 shl bit), 0, 3))
        assertFalse(WrappingPolicy.accepts(-1, 1, 15, 0, 3))
        assertFalse(WrappingPolicy.accepts(256, -1, 15, 0, 3))
        assertFalse(WrappingPolicy.accepts(256, 1, -1, 0, 3))
        assertFalse(WrappingPolicy.accepts(256, 1, 15, Int.MIN_VALUE, 3))
        assertFalse(WrappingPolicy.accepts(256, 1, 15, 0, -1))
    }
}
