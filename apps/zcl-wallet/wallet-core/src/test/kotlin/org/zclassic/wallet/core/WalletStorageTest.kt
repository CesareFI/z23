// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet.core

import java.nio.file.Files
import java.nio.file.Path
import java.nio.file.attribute.PosixFilePermissions
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNull
import kotlin.test.assertTrue

class WalletStorageTest {
    private fun record(): ByteArray {
        val entropy = ByteArray(16)
        return try {
            WalletRecord.pack(WalletRecord.createHeader(entropy, Network.TESTNET), ByteArray(12), ByteArray(32))
        } finally {
            entropy.fill(0)
        }
    }

    private fun withFixture(test: (Path, WalletStorage) -> Unit) {
        val path = Files.createTempDirectory("zcl-jni-storage-")
        try {
            test(path, WalletStorage(path.toString()))
        } finally {
            // Only names created by this test, inside its newly owned directory.
            listOf("wallet.zcl", ".wallet.pending", ".change.index", ".lock").forEach { Files.deleteIfExists(path.resolve(it)) }
            Files.delete(path)
        }
    }

    @Test fun nativeStorageCreatesReadsAndRefusesOverwrite() = withFixture { _, storage ->
        val bytes = record()
        val missing = storage.read()
        assertEquals(CoreStatus.NOT_FOUND, missing.status)
        assertNull(missing.record)
        assertEquals(CoreStatus.OK, storage.create(bytes))
        assertEquals(CoreStatus.ALREADY_EXISTS, storage.create(bytes))
        val read = storage.read()
        assertEquals(CoreStatus.OK, read.status)
        assertFalse(read.pending)
        assertContentEquals(bytes, read.record)
        assertEquals(CoreStatus.OK, storage.promote(bytes))
        bytes[bytes.lastIndex] = 1
        assertEquals(CoreStatus.ALREADY_EXISTS, storage.promote(bytes))
    }

    @Test fun pendingRecordIsExplicitAndRequiresExactPromotion() = withFixture { path, storage ->
        val bytes = record()
        val pending = path.resolve(".wallet.pending")
        Files.createFile(pending, PosixFilePermissions.asFileAttribute(PosixFilePermissions.fromString("rw-------")))
        Files.write(pending, bytes)
        val read = storage.read()
        assertEquals(CoreStatus.OK, read.status)
        assertTrue(read.pending)
        assertContentEquals(bytes, read.record)
        val changed = bytes.copyOf().also { it[it.lastIndex] = 1 }
        assertEquals(CoreStatus.INVALID_ENCODING, storage.promote(changed))
        assertEquals(CoreStatus.OK, storage.promote(bytes))
        assertFalse(storage.read().pending)
    }

    @Test fun jniRejectsOversizedPathsAndRecords() = withFixture { path, storage ->
        val bytes = record()
        assertEquals(CoreStatus.OUT_OF_RANGE, storage.create(ByteArray(141)))
        assertEquals(CoreStatus.OUT_OF_RANGE.code,
            NativeCore.readWalletStorage(ByteArray(0))?.first()?.toInt())
        assertEquals(CoreStatus.OUT_OF_RANGE.code,
            NativeCore.readWalletStorage(ByteArray(1025))?.first()?.toInt())
        assertEquals(CoreStatus.INVALID_ENCODING, WalletStorage(path.toString() + "/").create(bytes))
        assertEquals(CoreStatus.NOT_FOUND, storage.read().status)
    }

    @Test fun freshCreationPairsBothNetworksAndEveryEntropyWidth() {
        for (network in Network.entries) for (length in 16..32 step 4) {
            withFixture { path, storage ->
                val entropy = ByteArray(length)
                try {
                    val header = WalletRecord.createHeader(entropy, network)
                    val bytes = WalletRecord.pack(header, ByteArray(12), ByteArray(length + 16))
                    assertEquals(CoreStatus.OK, storage.createFreshWithChange(bytes, entropy))
                    assertContentEquals(bytes, storage.read().record)
                    val state = Files.readAllBytes(path.resolve(".change.index"))
                    assertEquals(80, state.size)
                    assertContentEquals(byteArrayOf(90, 67, 76, 73, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
                        state.copyOfRange(0, 16))
                    assertTrue(state.copyOfRange(16, 80).any { it.toInt() != 0 })
                    assertEquals(CoreStatus.ALREADY_EXISTS, storage.createFreshWithChange(bytes, entropy))
                    assertEquals(CoreStatus.ALREADY_EXISTS, storage.create(bytes))
                    assertContentEquals(state, Files.readAllBytes(path.resolve(".change.index")))
                } finally {
                    entropy.fill(0)
                }
            }
        }
    }

    @Test fun wrongEntropyAndOversizedJniInputsCannotCreateFiles() = withFixture { path, storage ->
        val bytes = record()
        val wrong = ByteArray(16) { 1 }
        try {
            assertEquals(CoreStatus.INVALID_ENCODING, storage.createFreshWithChange(bytes, wrong))
            assertEquals(CoreStatus.OUT_OF_RANGE, storage.createFreshWithChange(bytes, ByteArray(33)))
            assertEquals(CoreStatus.OUT_OF_RANGE, storage.createFreshWithChange(ByteArray(141), wrong))
            assertEquals(CoreStatus.OUT_OF_RANGE.code,
                NativeCore.createFreshWalletStorage(ByteArray(1025), bytes, wrong))
            assertEquals(CoreStatus.NOT_FOUND, storage.read().status)
            assertFalse(Files.exists(path.resolve(".change.index")))
            assertFalse(Files.exists(path.resolve(".wallet.pending")))
        } finally {
            wrong.fill(0)
        }
    }

    @Test fun freshCreationRefusesExistingWalletAndOrphanState() = withFixture { path, storage ->
        val bytes = record()
        val entropy = ByteArray(16)
        try {
            val orphan = path.resolve(".change.index")
            Files.createFile(orphan, PosixFilePermissions.asFileAttribute(PosixFilePermissions.fromString("rw-------")))
            Files.write(orphan, byteArrayOf(42))
            assertEquals(CoreStatus.ALREADY_EXISTS, storage.createFreshWithChange(bytes, entropy))
            assertEquals(CoreStatus.NOT_FOUND, storage.read().status)
            assertContentEquals(byteArrayOf(42), Files.readAllBytes(orphan))
            Files.delete(orphan) // Only this test's deliberately created orphan.
            assertEquals(CoreStatus.OK, storage.create(bytes))
            assertEquals(CoreStatus.ALREADY_EXISTS, storage.createFreshWithChange(bytes, entropy))
            assertFalse(Files.exists(orphan))
            assertContentEquals(bytes, storage.read().record)
        } finally {
            entropy.fill(0)
        }
    }
}
