// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith
import org.zclassic.wallet.core.Network
import org.zclassic.wallet.core.WalletKeys

/** Local test material only. No wallet file, hardware-policy bypass or funds. */
@RunWith(AndroidJUnit4::class)
class NativeKeysInstrumentedTest {
    @Test fun publicSecretDestinationsPreserveAllSupportedEntropyLengths() {
        for (length in listOf(16, 20, 24, 28, 32)) {
            val entropy = ByteArray(length) { it.toByte() } // Public synthetic vector.
            val words = WalletKeys.recoveryPhrase(entropy)
            try {
                assertTrue(words.isNotEmpty() && words.size <= 215)
                assertTrue(words.none { it == '\u0000' })
                assertEquals(length * 3 / 4, words.count { it == ' ' } + 1)
                val restored = WalletKeys.restoreEntropy(words)
                try {
                    assertEquals(length, restored.size)
                    assertTrue(entropy.contentEquals(restored))
                    assertTrue(WalletKeys.confirmRecoveryPhrase(entropy, words))
                } finally { restored.fill(0) }
                assertTrue(entropy.indices.all { entropy[it] == it.toByte() })
            } finally { entropy.fill(0); words.fill('\u0000') }
        }
    }

    @Test fun publicMnemonicCopiesAndRefusalsLeaveTheVmUsable() {
        val entropy = ByteArray(16)
        val phrase = WalletKeys.recoveryPhrase(entropy)
        try {
            assertEquals("abandon ".repeat(11) + "about", phrase.concatToString()) // Published vector only.
            val restored = WalletKeys.restoreEntropy(phrase)
            try { assertTrue(entropy.contentEquals(restored)) } finally { restored.fill(0) }
            assertTrue(WalletKeys.confirmRecoveryPhrase(entropy, phrase))
            val invalid = phrase.copyOf().also { it[0] = '\u0100' }
            try {
                assertThrows(IllegalArgumentException::class.java) { WalletKeys.restoreEntropy(invalid) }
                assertFalse(WalletKeys.confirmRecoveryPhrase(entropy, invalid))
            } finally { invalid.fill('\u0000') }
            assertThrows(IllegalArgumentException::class.java) { WalletKeys.recoveryPhrase(ByteArray(33)) }
            assertThrows(IllegalArgumentException::class.java) { WalletKeys.restoreEntropy(CharArray(216)) }
            for (network in Network.entries) {
                assertEquals(network, WalletKeys.receivingAddress(entropy, network).network)
            }
            assertTrue(WalletKeys.confirmRecoveryPhrase(entropy, phrase))
            assertTrue(entropy.all { it == 0.toByte() })
        } finally {
            entropy.fill(0)
            phrase.fill('\u0000')
        }
    }

    @Test fun freshTestEntropyRoundTripsWithoutConvertingSecretsToStrings() {
        val entropy = WalletKeys.createEntropy()
        try {
            assertEquals(16, entropy.size)
            val phrase = WalletKeys.recoveryPhrase(entropy)
            try {
                assertEquals(11, phrase.count { it == ' ' })
                val restored = WalletKeys.restoreEntropy(phrase)
                try { assertTrue(entropy.contentEquals(restored)) } finally { restored.fill(0) }
                assertTrue(WalletKeys.confirmRecoveryPhrase(entropy, phrase))
                val different = entropy.copyOf().also { it[0] = (it[0].toInt() xor 1).toByte() }
                try { assertFalse(WalletKeys.confirmRecoveryPhrase(different, phrase)) }
                finally { different.fill(0) }
            } finally { phrase.fill('\u0000') }
        } finally { entropy.fill(0) }
    }
}
