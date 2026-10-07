// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import java.nio.file.Files
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Test
import org.zclassic.wallet.core.CoreStatus
import org.zclassic.wallet.core.Network
import org.zclassic.wallet.core.WalletRecord
import org.zclassic.wallet.core.WalletStorage

class WalletPersistenceTest {
    private fun checkAction(action: WalletAction) {
        val directory = Files.createTempDirectory("zcl-action-storage-")
        val storage = WalletStorage(directory.toString())
        val entropy = ByteArray(16) { (it + 1).toByte() } // Public, inert test fixture.
        try {
            val encoded = WalletRecord.pack(WalletRecord.createHeader(entropy, Network.TESTNET),
                ByteArray(12), ByteArray(32))
            val status = commitPreparedWallet(storage, action, encoded, entropy)
            if (action == WalletAction.UNLOCK) {
                assertEquals(CoreStatus.INVALID_ARGUMENT, status)
                assertEquals(CoreStatus.NOT_FOUND, storage.read().status)
            } else {
                assertEquals(CoreStatus.OK, status)
                assertArrayEquals(encoded, storage.read().record)
                assertEquals(CoreStatus.ALREADY_EXISTS, commitPreparedWallet(storage, action, encoded, entropy))
            }
            if (action == WalletAction.CREATE) {
                assertEquals(80L, Files.size(directory.resolve(".change.index")))
            } else assertFalse(Files.exists(directory.resolve(".change.index")))
            // JNI borrows managed entropy; clearing its private C copy must not
            // mutate caller bytes before the worker's own finally block.
            assertArrayEquals(ByteArray(16) { (it + 1).toByte() }, entropy)
        } finally {
            entropy.fill(0)
            listOf("wallet.zcl", ".wallet.pending", ".change.index", ".lock").forEach {
                Files.deleteIfExists(directory.resolve(it))
            }
            Files.delete(directory)
        }
    }

    @Test fun freshCreatePersistsPairedState() = checkAction(WalletAction.CREATE)
    @Test fun restorationDoesNotResetHistoricalChange() = checkAction(WalletAction.RESTORE)
    @Test fun unlockCannotEnterCreation() = checkAction(WalletAction.UNLOCK)
}
