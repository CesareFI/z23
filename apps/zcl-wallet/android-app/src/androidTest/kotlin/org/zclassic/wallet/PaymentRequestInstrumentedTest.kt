// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.zclassic.wallet.core.Network
import org.zclassic.wallet.core.PaymentRequest
import org.zclassic.wallet.core.Zatoshi

/** Fixed public requests only; no camera, wallet, key or network operation. */
@RunWith(AndroidJUnit4::class)
class PaymentRequestInstrumentedTest {
    private val addresses = listOf(
        "t1T8yaLVhNqxA5KJcmiqqFN88e8DNp2PBfF" to Network.MAINNET,
        "t3VDyGHn9mbyCf448m2cHTu5uXvsJpKHbiZ" to Network.MAINNET,
        "tmHMBeeYRuc2eVicLNfP15YLxbQsooCA6jb" to Network.TESTNET,
        "t2Fbo6DBKKVYw1SfrY8bEgz56hYEhywhEN6" to Network.TESTNET,
    )

    @Test fun maximumPaymentPacketHasExactAmountUtf8AndIndependentOwnedFields() {
        val label = "é".repeat(100) // 200 UTF-8 bytes, not 200 characters.
        val message = "M".repeat(200)
        for ((address, network) in addresses) {
            val input = ("zclassic:$address?amount=21000000&label=" +
                "%C3%A9".repeat(100) + "&message=$message").toByteArray(Charsets.US_ASCII)
            val original = input.copyOf()
            val parsed = checkNotNull(PaymentRequest.parseEncoded(input, network))
            assertTrue(input.contentEquals(original))
            input.fill(0)
            assertEquals(address, parsed.address.encoded)
            assertEquals(network, parsed.address.network)
            assertEquals(Zatoshi.of(Zatoshi.MAX_VALUE), parsed.amount)
            assertEquals(label, parsed.label)
            assertEquals(message, parsed.message)
        }
    }

    @Test fun malformedAndWrongNetworkCallsDoNotPoisonTheNextPublicRequest() {
        for ((address, network) in addresses) {
            val wrong = if (network == Network.MAINNET) Network.TESTNET else Network.MAINNET
            assertNull(PaymentRequest.parseEncoded(address.toByteArray(Charsets.US_ASCII), wrong))
            for (query in listOf("label=" + "x".repeat(201), "label=%ff", "amount=1&amount=2",
                "amount=21000000.00000001", "label=%00", "message=%E2%80%AEhidden")) {
                val input = "zclassic:$address?$query".toByteArray(Charsets.US_ASCII)
                val original = input.copyOf()
                assertNull(PaymentRequest.parseEncoded(input, network))
                assertTrue(input.contentEquals(original))
                val fresh = PaymentRequest.parse(address, network)
                assertEquals(address, fresh.address.encoded)
                assertNull(fresh.amount)
                assertNull(fresh.label)
                assertNull(fresh.message)
            }
        }
    }
}
