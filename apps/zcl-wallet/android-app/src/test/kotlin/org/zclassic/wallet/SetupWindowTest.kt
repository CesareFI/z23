// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import org.junit.Assert.assertEquals
import org.junit.Assert.assertThrows
import org.junit.Test

class SetupWindowTest {
    @Test fun queueDelayAndRepeatedUseCannotExtendSetup() {
        var now = 100L
        val window = SetupWindow { now }
        assertEquals(600_000L, window.remainingMillis)
        window.requireOpen()
        now = 600_099
        assertEquals(1L, window.remainingMillis)
        window.requireOpen()
        now = 600_100
        repeat(3) {
            assertEquals(0L, window.remainingMillis)
            assertThrows(IllegalStateException::class.java) { window.requireOpen() }
        }
        now = Long.MAX_VALUE
        assertEquals(0L, window.remainingMillis)
    }

    @Test fun backwardOrNegativeClockRefusesAndOnlyNewSetupGetsANewOrigin() {
        var now = 100L
        val window = SetupWindow { now }
        now = 99
        assertThrows(IllegalStateException::class.java) { window.requireOpen() }
        now = -1
        assertEquals(0L, window.remainingMillis)
        val invalid = SetupWindow { now }
        assertThrows(IllegalStateException::class.java) { invalid.requireOpen() }
        now = 600_100
        val replacement = SetupWindow { now }
        assertEquals(600_000L, replacement.remainingMillis)
        assertEquals(0L, window.remainingMillis)
    }
}
