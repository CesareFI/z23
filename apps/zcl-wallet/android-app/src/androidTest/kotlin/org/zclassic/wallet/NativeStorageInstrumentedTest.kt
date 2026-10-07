// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import java.io.File
import java.util.UUID
import javax.crypto.AEADBadTagException
import javax.crypto.Cipher
import javax.crypto.spec.GCMParameterSpec
import javax.crypto.spec.SecretKeySpec
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertThrows
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.zclassic.wallet.core.CoreStatus
import org.zclassic.wallet.core.Network
import org.zclassic.wallet.core.WalletKeys
import org.zclassic.wallet.core.WalletRecord
import org.zclassic.wallet.core.WalletStorage

@RunWith(AndroidJUnit4::class)
class NativeStorageInstrumentedTest {
    @Test fun nativeStorageAndProviderGcmRoundTripOnAndroid() = roundTrip(false)

    @Test fun freshPairedStorageAndProviderGcmRoundTripOnAndroid() = roundTrip(true)

    private fun roundTrip(fresh: Boolean) {
        val context = InstrumentationRegistry.getInstrumentation().targetContext
        val directory = File(context.noBackupFilesDir, "instrumentation-" + UUID.randomUUID().toString())
        assertFalse(directory.exists())
        val storage = WalletStorage(directory.absolutePath)
        val entropy = ByteArray(16) // Published test fixture; never a funded wallet.
        try {
            assertEquals(CoreStatus.NOT_FOUND, storage.read().status)
            val header = WalletRecord.createHeader(entropy, Network.TESTNET)
            val key = SecretKeySpec(ByteArray(32), "AES") // Public test-only AES key.
            val encrypt = Cipher.getInstance("AES/GCM/NoPadding")
            encrypt.init(Cipher.ENCRYPT_MODE, key)
            encrypt.updateAAD(header)
            val bytes = WalletRecord.pack(header, encrypt.iv, encrypt.doFinal(entropy))
            val action = if (fresh) WalletAction.CREATE else WalletAction.RESTORE
            assertEquals(CoreStatus.OK, commitPreparedWallet(storage, action, bytes, entropy))
            assertEquals(CoreStatus.ALREADY_EXISTS, storage.create(bytes))
            val state = File(directory, ".change.index")
            if (fresh) {
                assertEquals(80L, state.length())
                assertArrayEquals(byteArrayOf(90, 67, 76, 73, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
                    state.readBytes().copyOfRange(0, 16))
                assertEquals(CoreStatus.ALREADY_EXISTS, storage.createFreshWithChange(bytes, entropy))
            } else assertFalse(state.exists())
            val stored = storage.read()
            assertEquals(CoreStatus.OK, stored.status)
            assertFalse(stored.pending)
            assertArrayEquals(bytes, stored.record)
            val parsed = WalletRecord.parse(checkNotNull(stored.record))
            val decrypt = Cipher.getInstance("AES/GCM/NoPadding")
            decrypt.init(Cipher.DECRYPT_MODE, key, GCMParameterSpec(128, parsed.iv))
            decrypt.updateAAD(parsed.header)
            val recovered = decrypt.doFinal(parsed.ciphertext)
            try {
                assertTrue(entropy.contentEquals(recovered))
                assertEquals(WalletKeys.receivingAddress(entropy, Network.TESTNET),
                    WalletRecord.recoveredAddress(parsed.header, recovered, parsed.network))
            } finally {
                recovered.fill(0)
            }
            for (index in parsed.header.indices) {
                val changed = parsed.header.copyOf().also { it[index] = (it[index].toInt() xor 1).toByte() }
                val attempt = Cipher.getInstance("AES/GCM/NoPadding")
                attempt.init(Cipher.DECRYPT_MODE, key, GCMParameterSpec(128, parsed.iv))
                attempt.updateAAD(changed)
                assertThrows(AEADBadTagException::class.java) { attempt.doFinal(parsed.ciphertext) }
            }
            val changedCiphertext = parsed.ciphertext.copyOf().also { it[0] = (it[0].toInt() xor 1).toByte() }
            val attempt = Cipher.getInstance("AES/GCM/NoPadding")
            attempt.init(Cipher.DECRYPT_MODE, key, GCMParameterSpec(128, parsed.iv))
            attempt.updateAAD(parsed.header)
            assertThrows(AEADBadTagException::class.java) { attempt.doFinal(changedCiphertext) }
        } finally {
            entropy.fill(0)
            // This test created this directory; never touch the real wallet name.
            listOf("wallet.zcl", ".wallet.pending", ".change.index", ".lock").forEach { name ->
                val file = File(directory, name)
                if (file.exists()) assertTrue(file.delete())
            }
            if (directory.exists()) assertTrue(directory.delete())
        }
    }
}
