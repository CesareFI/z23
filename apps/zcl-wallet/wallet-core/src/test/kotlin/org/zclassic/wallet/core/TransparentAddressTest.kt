// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet.core

import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith

class TransparentAddressTest {
    // Public fixtures from zclassic/src/test/data/base58_keys_valid.json.
    private val fixtures = listOf(
        Triple("t1T8yaLVhNqxA5KJcmiqqFN88e8DNp2PBfF", Network.MAINNET,
            "76a91465a16059864a2fdbc7c99a4723a8395bc6f188eb88ac"),
        Triple("t3VDyGHn9mbyCf448m2cHTu5uXvsJpKHbiZ", Network.MAINNET,
            "a91474f209f6ea907e2ea48f74fae05782ae8a66525787"),
        Triple("tmHMBeeYRuc2eVicLNfP15YLxbQsooCA6jb", Network.TESTNET,
            "76a91453c0307d6851aa0ce7825ba883c6bd9ad242b48688ac"),
        Triple("t2Fbo6DBKKVYw1SfrY8bEgz56hYEhywhEN6", Network.TESTNET,
            "a9146349a418fc4578d10a372b54b45c280cc8c4382f87")
    )

    @Test fun referenceAddressesProduceExactScripts() {
        fixtures.forEach { (encoded, network, script) ->
            val address = TransparentAddress.parse(encoded, network)
            assertEquals(encoded, address.encoded)
            assertContentEquals(script.hexToByteArray(), address.scriptPubKey())
        }
    }

    @Test fun wrongNetworkAndChecksumAreRejected() {
        fixtures.forEach { (encoded, network, _) ->
            val wrong = if (network == Network.MAINNET) Network.TESTNET else Network.MAINNET
            assertFailsWith<IllegalArgumentException> { TransparentAddress.parse(encoded, wrong) }
            encoded.indices.forEach { index ->
                val changed = encoded.toCharArray().apply { this[index] = if (this[index] == '1') '2' else '1' }
                assertFailsWith<IllegalArgumentException> {
                    TransparentAddress.parse(String(changed), network)
                }
            }
        }
    }

    @Test fun invalidLengthCharactersAndWhitespaceAreRejected() {
        listOf("", "0OIl", "1".repeat(4096), " ${fixtures[0].first}",
            fixtures[0].first + "\n").forEach {
            assertFailsWith<IllegalArgumentException> { TransparentAddress.parse(it, Network.MAINNET) }
        }
        listOf(0, 19, 21, 30).forEach { size ->
            val encoded = Base58Check.encode(byteArrayOf(0x1c, 0xb8.toByte()) + ByteArray(size))
            assertFailsWith<IllegalArgumentException> { TransparentAddress.parse(encoded, Network.MAINNET) }
        }
    }

    @Test fun unknownVersionRejectedEvenWithValidChecksum() {
        val encoded = Base58Check.encode(byteArrayOf(0, 0) + ByteArray(20))
        assertFailsWith<IllegalArgumentException> { TransparentAddress.parse(encoded, Network.MAINNET) }
    }

    @Test fun scriptArraysDoNotExposeMutableAddressState() {
        val address = TransparentAddress.parse(fixtures[0].first, Network.MAINNET)
        address.scriptPubKey().fill(0)
        assertContentEquals(fixtures[0].third.hexToByteArray(), address.scriptPubKey())
    }
}
