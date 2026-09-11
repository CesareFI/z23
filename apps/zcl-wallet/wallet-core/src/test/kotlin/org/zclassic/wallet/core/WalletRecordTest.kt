// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet.core

import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertNull

class WalletRecordTest {
    @Test fun recordPreservesProviderInputsAndRechecksRecoveredAddress() {
        val entropy = ByteArray(16) // Published all-zero test fixture only.
        try {
            for (network in Network.entries) {
                val header = WalletRecord.createHeader(entropy, network)
                val iv = ByteArray(12) { it.toByte() }
                val ciphertext = ByteArray(32) { (it + 7).toByte() }
                val packed = WalletRecord.pack(header, iv, ciphertext)
                val parsed = WalletRecord.parse(packed)
                assertContentEquals(header, parsed.header)
                assertContentEquals(iv, parsed.iv)
                assertContentEquals(ciphertext, parsed.ciphertext)
                assertEquals(network, parsed.network)
                // This checks fixture/header agreement, not GCM authentication.
                assertEquals(WalletKeys.receivingAddress(entropy, network),
                    WalletRecord.recoveredAddress(parsed.header, entropy, network))
                assertContentEquals(packed, WalletRecord.pack(parsed.header, parsed.iv, parsed.ciphertext))
            }
        } finally {
            entropy.fill(0)
        }
    }

    @Test fun malformedRecordsAndMismatchedRecoveryFail() {
        val entropy = ByteArray(32)
        try {
            val header = WalletRecord.createHeader(entropy, Network.MAINNET)
            val packed = WalletRecord.pack(header, ByteArray(12), ByteArray(48))
            assertFailsWith<IllegalArgumentException> { WalletRecord.pack(header, ByteArray(13), ByteArray(48)) }
            for (size in 0 until packed.size)
                assertFailsWith<IllegalArgumentException> { WalletRecord.parse(packed.copyOf(size)) }
            assertFailsWith<IllegalArgumentException> { WalletRecord.parse(packed + 0) }
            entropy[0] = 1
            assertFailsWith<IllegalArgumentException> { WalletRecord.recoveredAddress(header, entropy, Network.MAINNET) }
            assertNull(NativeCore.createWalletHeader(ByteArray(33), 0))
            assertNull(NativeCore.createWalletHeader(ByteArray(16), -1))
            assertNull(NativeCore.recoveredWalletAddress(ByteArray(81), entropy))
        } finally {
            entropy.fill(0)
        }
    }
}
