// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet.core

import kotlin.test.Test
import kotlin.test.assertFalse
import kotlin.test.assertTrue

class WrappingPolicyTest {
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
