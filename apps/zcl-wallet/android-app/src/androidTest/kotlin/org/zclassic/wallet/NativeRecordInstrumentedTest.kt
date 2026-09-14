// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import androidx.test.ext.junit.runners.AndroidJUnit4
import javax.crypto.AEADBadTagException
import javax.crypto.Cipher
import javax.crypto.spec.GCMParameterSpec
import javax.crypto.spec.SecretKeySpec
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertThrows
import org.junit.Test
import org.junit.runner.RunWith
import org.zclassic.wallet.core.Network
import org.zclassic.wallet.core.WalletKeys
import org.zclassic.wallet.core.WalletRecord

/** Public vectors in memory only; no wallet directory or Keystore alias. */
@RunWith(AndroidJUnit4::class)
class NativeRecordInstrumentedTest {
    @Test fun everyRecordSizeAndNetworkHasIndependentJniArraysAndProviderAuthentication() {
        for (network in Network.entries) {
            for (size in listOf(16, 20, 24, 28, 32)) verifyRecord(network, size)
        }
    }

    private fun verifyRecord(network: Network, size: Int) {
        val entropy = ByteArray(size) // Fixed public vector, never a funded wallet.
        try {
            val header = WalletRecord.createHeader(entropy, network)
            val key = SecretKeySpec(ByteArray(32), "AES") // Public test key only.
            val encrypt = Cipher.getInstance("AES/GCM/NoPadding")
            encrypt.init(Cipher.ENCRYPT_MODE, key)
            encrypt.updateAAD(header)
            val iv = encrypt.iv
            val ciphertext = encrypt.doFinal(entropy)
            val bytes = WalletRecord.pack(header, iv, ciphertext)
            assertEquals(108 + size, bytes.size)
            assertArrayEquals(header + iv + ciphertext, bytes)
            val saved = bytes.copyOf()
            val parsed = WalletRecord.parse(bytes)
            assertEquals(network, parsed.network)
            assertArrayEquals(header, parsed.header)
            assertArrayEquals(iv, parsed.iv)
            assertArrayEquals(ciphertext, parsed.ciphertext)
            parsed.header.fill(0)
            parsed.iv.fill(0)
            parsed.ciphertext.fill(0)
            assertArrayEquals(saved, bytes)
            assertArrayEquals(saved, header + iv + ciphertext)
            val independent = WalletRecord.parse(bytes)
            assertArrayEquals(header, independent.header)
            assertArrayEquals(iv, independent.iv)
            assertArrayEquals(ciphertext, independent.ciphertext)
            verifyRefusals(bytes, independent, key)
            val recovered = decrypt(WalletRecord.parse(bytes), key)
            try {
                assertArrayEquals(entropy, recovered)
                assertEquals(WalletKeys.receivingAddress(entropy, network),
                    WalletRecord.recoveredAddress(independent.header, recovered, independent.network))
            } finally { recovered.fill(0) }
            assertArrayEquals(saved, bytes)
        } finally { entropy.fill(0) }
    }

    private fun verifyRefusals(bytes: ByteArray, parsed: WalletRecord, key: SecretKeySpec) {
        for (length in listOf(0, bytes.size - 1, bytes.size + 1, 141)) {
            assertThrows(IllegalArgumentException::class.java) { WalletRecord.parse(bytes.copyOf(length)) }
        }
        val badVersion = bytes.copyOf().also { it[4] = 0 }
        assertThrows(IllegalArgumentException::class.java) { WalletRecord.parse(badVersion) }
        assertThrows(IllegalArgumentException::class.java) {
            WalletRecord.pack(parsed.header, parsed.iv.copyOf(11), parsed.ciphertext)
        }
        assertThrows(IllegalArgumentException::class.java) {
            WalletRecord.pack(parsed.header, parsed.iv, parsed.ciphertext.copyOf(parsed.ciphertext.size - 1))
        }
        val changed = bytes.copyOf().also { it[it.lastIndex] = (it.last().toInt() xor 1).toByte() }
        // Structural parsing cannot grant authenticity; the actual provider must refuse the tag.
        val unauthenticated = WalletRecord.parse(changed)
        assertThrows(AEADBadTagException::class.java) { decrypt(unauthenticated, key).fill(0) }
    }

    private fun decrypt(record: WalletRecord, key: SecretKeySpec): ByteArray {
        val cipher = Cipher.getInstance("AES/GCM/NoPadding")
        cipher.init(Cipher.DECRYPT_MODE, key, GCMParameterSpec(128, record.iv))
        cipher.updateAAD(record.header)
        return cipher.doFinal(record.ciphertext)
    }
}
