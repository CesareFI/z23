// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import org.junit.Assert.*
import org.junit.Test

class UnlockWindowTest {
    @Test fun queueAndProviderTimeShareOneNonrenewableOrigin() {
        var now = 100L
        val window = UnlockWindow { now }
        window.requireOpen()
        now = 90_099
        window.requireOpen()
        now = 90_100
        repeat(3) {
            assertFalse(window.isOpen)
            assertThrows(IllegalStateException::class.java) { window.requireOpen() }
        }
        now = Long.MAX_VALUE
        assertFalse(window.isOpen)
    }

    @Test fun invalidClocksRefuseAndOnlyAnotherOperationGetsAnotherOrigin() {
        var now = 100L
        val original = UnlockWindow { now }
        now = 99
        assertFalse(original.isOpen)
        now = -1
        assertFalse(original.isOpen)
        val invalid = UnlockWindow { now }
        now = 90_100
        assertFalse(invalid.isOpen)
        assertFalse(original.isOpen)
        UnlockWindow { now }.requireOpen()
    }
}
