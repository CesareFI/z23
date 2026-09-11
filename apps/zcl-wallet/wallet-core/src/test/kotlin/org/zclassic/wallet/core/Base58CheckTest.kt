// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet.core

import java.util.Random
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith

class Base58CheckTest {
    @Test fun knownBitcoinPublicAddressChecksums() {
        val hash = "0065a16059864a2fdbc7c99a4723a8395bc6f188eb".hexToByteArray()
        assertEquals("1AGNa15ZQXAZUgFiqJ2i7Z2DPU2J6hW62i", Base58Check.encode(hash))
        assertContentEquals(hash, Base58Check.decode("1AGNa15ZQXAZUgFiqJ2i7Z2DPU2J6hW62i"))
    }

    @Test fun boundedRandomPublicPayloadRoundTrips() {
        val random = Random(0x5ac1)
        repeat(512) {
            val payload = ByteArray(1 + random.nextInt(128)).also(random::nextBytes)
            assertContentEquals(payload, Base58Check.decode(Base58Check.encode(payload)))
        }
        for (size in 1..128) {
            val zeroes = ByteArray(size)
            assertContentEquals(zeroes, Base58Check.decode(Base58Check.encode(zeroes)))
        }
    }

    @Test fun rejectsBoundsAndMissingChecksums() {
        listOf("", "1111", "1".repeat(185), "a\u0000b", "abcd0").forEach {
            assertFailsWith<IllegalArgumentException> { Base58Check.decode(it) }
        }
        assertFailsWith<IllegalArgumentException> { Base58Check.encode(ByteArray(129)) }
        assertFailsWith<IllegalArgumentException> { Base58Check.encode(ByteArray(0)) }
    }
}
