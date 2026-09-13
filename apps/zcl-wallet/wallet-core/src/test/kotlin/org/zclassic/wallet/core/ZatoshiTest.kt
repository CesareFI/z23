// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet.core

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith

class ZatoshiTest {
    @Test fun exactDecimalAmounts() {
        mapOf("0" to 0L, "0.00000001" to 1L, "1" to 100_000_000L,
            "12.34567890" to 1_234_567_890L, "21000000" to Zatoshi.MAX_VALUE)
            .forEach { (text, expected) -> assertEquals(expected, Zatoshi.parse(text).value) }
    }

    @Test fun rejectsAmbiguousOrOutOfRangeAmounts() {
        listOf("", " 1", "1 ", "+1", "-1", "01", "1.", ".1", "1e2", "1,2",
            "NaN", "Infinity", "１", "1.000000001", "21000000.00000001",
            "99999999999999999999999999999", "1\n").forEach {
            assertFailsWith<IllegalArgumentException>(it) { Zatoshi.parse(it) }
        }
    }

    @Test fun checkedArithmetic() {
        val maximum = Zatoshi.of(Zatoshi.MAX_VALUE)
        assertFailsWith<IllegalArgumentException> { maximum + Zatoshi.of(1) }
        assertFailsWith<IllegalArgumentException> { Zatoshi.ZERO - Zatoshi.of(1) }
        assertFailsWith<IllegalArgumentException> { Zatoshi.of(Long.MAX_VALUE) }
        assertEquals(Zatoshi.of(1), Zatoshi.of(3) - Zatoshi.of(2))
    }

    @Test fun canonicalFormattingRoundTrips() {
        listOf(0L, 1L, 10L, 100_000_000L, 123_456_789L, Zatoshi.MAX_VALUE).forEach {
            val amount = Zatoshi.of(it)
            assertEquals(amount, Zatoshi.parse(amount.format()))
        }
        assertEquals("0.00000001", Zatoshi.of(1).format())
        assertEquals("1", Zatoshi.of(100_000_000).format())
    }

    @Test fun signedChangesAreExactAndCannotBecomePaymentAmounts() {
        val cases = mapOf(0L to "0", 1L to "+0.00000001", -1L to "-0.00000001",
            100_000_000L to "+1", -123_456_789L to "-1.23456789",
            Zatoshi.MAX_VALUE to "+21000000", -Zatoshi.MAX_VALUE to "-21000000",
            Zatoshi.MAX_VALUE - 1 to "+20999999.99999999",
            1 - Zatoshi.MAX_VALUE to "-20999999.99999999")
        cases.forEach { (value, expected) ->
            assertEquals(expected, Zatoshi.formatDelta(value))
            if (value != 0L) assertFailsWith<IllegalArgumentException> { Zatoshi.parse(expected) }
        }
        for (value in listOf(Long.MIN_VALUE, Long.MAX_VALUE,
                             Zatoshi.MAX_VALUE + 1, -Zatoshi.MAX_VALUE - 1))
            assertFailsWith<IllegalArgumentException> { Zatoshi.formatDelta(value) }
    }
}
