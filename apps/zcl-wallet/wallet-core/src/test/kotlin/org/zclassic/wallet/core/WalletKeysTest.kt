// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet.core

import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertNotEquals

class WalletKeysTest {
    @Test fun publishedPhraseRoundTripsThroughCharacters() {
        val entropy = ByteArray(16)
        val expected = "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon about".toCharArray()
        val phrase = WalletKeys.recoveryPhrase(entropy)
        val restored = WalletKeys.restoreEntropy(phrase)
        try {
            assertContentEquals(expected, phrase)
            assertContentEquals(entropy, restored)
        } finally {
            entropy.fill(0)
            expected.fill('\u0000')
            phrase.fill('\u0000')
            restored.fill(0)
        }
    }

    @Test fun newEntropyComesFromNativeOsRngAndRestores() {
        val entropy = WalletKeys.createEntropy()
        var phrase: CharArray? = null
        var restored: ByteArray? = null
        try {
            val encoded = WalletKeys.recoveryPhrase(entropy).also { phrase = it }
            val decoded = WalletKeys.restoreEntropy(encoded).also { restored = it }
            assertEquals(32, entropy.size)
            assertEquals(24, encoded.count { it == ' ' } + 1)
            // Avoid an assertion framework that could print generated secrets.
            check(entropy.contentEquals(decoded)) { "Generated recovery round trip failed" }
        } finally {
            entropy.fill(0)
            phrase?.fill('\u0000')
            restored?.fill(0)
        }
    }

    @Test fun nativeDerivationPreservesNetworkAndIndex() {
        val publicFixture = ByteArray(16)
        try {
            val first = WalletKeys.receivingAddress(publicFixture, Network.MAINNET)
            val repeated = WalletKeys.receivingAddress(publicFixture, Network.MAINNET)
            val next = WalletKeys.receivingAddress(publicFixture, Network.MAINNET, 1)
            val testnet = WalletKeys.receivingAddress(publicFixture, Network.TESTNET)
            assertEquals(first, repeated) // Independent RNG blinding has no effect on the address.
            assertNotEquals(first, next)
            assertEquals(Network.TESTNET, testnet.network)
            assertNotEquals(first, testnet)
            assertFailsWith<IllegalStateException> {
                WalletKeys.receivingAddress(publicFixture, Network.MAINNET, -1)
            }
        } finally {
            publicFixture.fill(0)
        }
    }

    @Test fun malformedSecretInputsAreBoundedAndNotEchoed() {
        val invalid = listOf(CharArray(216) { 'a' }, charArrayOf('\uD800'), "abandon".toCharArray())
        for (phrase in invalid) {
            val failure = assertFailsWith<IllegalArgumentException> { WalletKeys.restoreEntropy(phrase) }
            assertEquals("Invalid or unsupported recovery phrase", failure.message)
            phrase.fill('\u0000')
        }
        assertFailsWith<IllegalArgumentException> { WalletKeys.recoveryPhrase(ByteArray(33)) }
    }
}
