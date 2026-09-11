// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet.core

import java.util.Random
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertFalse
import kotlin.test.assertNull

class PaymentRequestTest {
    private val address = "t1T8yaLVhNqxA5KJcmiqqFN88e8DNp2PBfF"
    private fun parse(text: String) = PaymentRequest.parse(text, Network.MAINNET)

    @Test fun acceptsOnlyExplicitZclassicOrBareAddress() {
        assertEquals(address, parse(address).address.encoded)
        assertNull(parse(address).amount)
        val request = parse("zclassic:$address?amount=1.00000001&label=Caf%C3%A9&message=Hello%20world")
        assertEquals(Zatoshi.of(100_000_001), request.amount)
        assertEquals("Café", request.label)
        assertEquals("Hello world", request.message)
        assertEquals("a+b", parse("zclassic:$address?label=a+b").label)
    }

    @Test fun rejectsHiddenDuplicateAndUnsupportedFields() {
        listOf("amount=1&amount=2", "amount=1&%61mount=2", "req-feature=yes", "r=https://example.com",
            "amount.1=2", "amount=0", "amount=-1", "amount=1e6", "label=%00", "label=%0a",
            "label=%E2%80%AEhidden", "label=%ff", "label=%c0%af", "label=%", "label=x&&message=y",
            "label=" + "x".repeat(201), "label", "", "amount=1;message=changed").forEach { query ->
            assertFailsWith<IllegalArgumentException>(query) { parse("zclassic:$address?$query") }
        }
    }

    @Test fun rejectsOtherChainsAndUriAuthority() {
        listOf("zcash:$address", "bitcoin:$address", "https://example.com/$address",
            "zclassic://$address", "zclassic:$address#fragment", "zclassic:$address?label=raw space",
            "zclassic:$address?label=rawé", " $address", "$address\n").forEach {
            assertFailsWith<IllegalArgumentException> { parse(it) }
        }
    }

    @Test fun noMalformedInputEscapesAsUnexpectedExceptionOrLeaksInput() {
        val random = Random(0x51c)
        repeat(2000) {
            val text = buildString { repeat(1 + random.nextInt(1200)) { append(random.nextInt(65536).toChar()) } }
            val error = assertFailsWith<IllegalArgumentException> { parse(text) }
            assertFalse(error.message.orEmpty().contains(text))
        }
    }
}
