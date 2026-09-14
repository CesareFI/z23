// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.system.ErrnoException
import android.system.Os
import android.system.OsConstants
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import java.io.Closeable
import java.io.File
import java.io.FileOutputStream
import java.nio.file.Files
import java.util.concurrent.CountDownLatch
import java.util.concurrent.Executors
import java.util.concurrent.TimeUnit
import javax.crypto.AEADBadTagException
import javax.crypto.Cipher
import javax.crypto.spec.GCMParameterSpec
import javax.crypto.spec.SecretKeySpec
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertThrows
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.zclassic.wallet.core.CoreStatus
import org.zclassic.wallet.core.Network
import org.zclassic.wallet.core.WalletRecord
import org.zclassic.wallet.core.WalletStorage

/** Public provider-GCM records in exclusively created temporary directories.
 * This qualifies native Android file behavior, not hardware custody or power loss.
 * No Keystore alias, actual wallet directory, endpoint or production state.
 */
@RunWith(AndroidJUnit4::class)
class NativeStorageRecoveryInstrumentedTest {
    private class Fixture : Closeable {
        private val context = InstrumentationRegistry.getInstrumentation().targetContext
        val root: File = Files.createTempDirectory(context.noBackupFilesDir.toPath(), "storage-contract-").toFile()
        val directory = File(root, "store")
        val storage = WalletStorage(directory.absolutePath)
        fun file(name: String) = File(directory, name)

        fun prepare() {
            Os.chmod(root.absolutePath, 0x1c0) // 0700; only this invocation owns it.
            assertEquals(CoreStatus.NOT_FOUND, storage.read().status)
            assertEquals(0x1c0, Os.stat(directory.absolutePath).st_mode and 0x1ff)
            assertEquals(0x180, Os.stat(file(".lock").absolutePath).st_mode and 0x1ff)
        }

        fun write(name: String, bytes: ByteArray) {
            assertTrue(name in listOf("wallet.zcl", ".wallet.pending", ".lock"))
            val target = file(name)
            assertFalse(Files.isSymbolicLink(target.toPath()))
            FileOutputStream(target).use { it.write(bytes); it.fd.sync() }
            Os.chmod(target.absolutePath, 0x180) // 0600, independent of runtime umask.
        }

        fun noOwnedDescriptors() {
            val entries = checkNotNull(File("/proc/self/fd").list())
            assertTrue(entries.size <= 4096)
            for (entry in entries) {
                val target = try { Os.readlink("/proc/self/fd/$entry") }
                catch (problem: ErrnoException) {
                    if (problem.errno != OsConstants.ENOENT) throw problem
                    continue // A platform descriptor closed during enumeration.
                }
                assertFalse("Native call retained a fixture descriptor",
                    target == root.absolutePath || target.startsWith(root.absolutePath + "/"))
            }
        }

        override fun close() {
            try { noOwnedDescriptors() }
            finally {
                // Fixed invocation-owned names only; delete never follows links.
                for (name in listOf("wallet.zcl", ".wallet.pending", ".lock"))
                    Files.deleteIfExists(file(name).toPath())
                Files.deleteIfExists(File(root, "alias").toPath())
                Files.deleteIfExists(File(root, "record-target").toPath())
                Files.deleteIfExists(directory.toPath())
                Files.delete(root.toPath())
            }
        }
    }

    private fun publicRecord(): ByteArray {
        val entropy = ByteArray(16)
        try {
            val header = WalletRecord.createHeader(entropy, Network.TESTNET)
            val cipher = Cipher.getInstance("AES/GCM/NoPadding")
            cipher.init(Cipher.ENCRYPT_MODE, SecretKeySpec(ByteArray(32), "AES"))
            cipher.updateAAD(header)
            return WalletRecord.pack(header, cipher.iv, cipher.doFinal(entropy))
        } finally { entropy.fill(0) }
    }

    private fun authenticatePublicRecord(bytes: ByteArray) {
        val record = WalletRecord.parse(bytes)
        val cipher = Cipher.getInstance("AES/GCM/NoPadding")
        cipher.init(Cipher.DECRYPT_MODE, SecretKeySpec(ByteArray(32), "AES"), GCMParameterSpec(128, record.iv))
        cipher.updateAAD(record.header)
        val recovered = cipher.doFinal(record.ciphertext)
        try {
            assertEquals(Network.TESTNET, WalletRecord.recoveredAddress(record.header, recovered, record.network).network)
        } finally { recovered.fill(0) }
    }

    @Test fun authenticatedPendingPromotionBindsExactBytesAndNeverReplacesACompetitor() = Fixture().use { fixture ->
        fixture.prepare()
        val original = publicRecord()
        val competitor = publicRecord()
        assertFalse(original.contentEquals(competitor))
        fixture.write(".wallet.pending", original)
        val pending = fixture.storage.read()
        assertEquals(CoreStatus.OK, pending.status)
        assertTrue(pending.pending)
        assertArrayEquals(original, pending.record)
        authenticatePublicRecord(checkNotNull(pending.record))
        authenticatePublicRecord(competitor)
        assertEquals(CoreStatus.INVALID_ENCODING, fixture.storage.promote(competitor))
        assertArrayEquals(original, fixture.file(".wallet.pending").readBytes())
        assertFalse(fixture.file("wallet.zcl").exists())
        assertEquals(CoreStatus.OK, fixture.storage.promote(original))
        assertFalse(fixture.file(".wallet.pending").exists())
        assertArrayEquals(original, fixture.file("wallet.zcl").readBytes())
        assertEquals(CoreStatus.OK, fixture.storage.promote(original))
        assertEquals(CoreStatus.ALREADY_EXISTS, fixture.storage.promote(competitor))
        val committed = fixture.storage.read()
        assertEquals(CoreStatus.OK, committed.status)
        assertFalse(committed.pending)
        assertArrayEquals(original, committed.record)
    }

    @Test fun badPendingTagCannotPassPublicGcmRecoveryAndCreationPreservesIt() = Fixture().use { fixture ->
        fixture.prepare()
        val original = publicRecord()
        val damaged = original.copyOf().also { it[it.lastIndex] = (it.last().toInt() xor 1).toByte() }
        fixture.write(".wallet.pending", damaged)
        val pending = fixture.storage.read()
        assertEquals(CoreStatus.OK, pending.status) // Structure is not authentication.
        assertTrue(pending.pending)
        assertThrows(AEADBadTagException::class.java) { authenticatePublicRecord(checkNotNull(pending.record)) }
        // No promotion follows failed authentication, even with complete bytes.
        assertEquals(CoreStatus.ALREADY_EXISTS, fixture.storage.create(original))
        assertFalse(fixture.file("wallet.zcl").exists())
        assertArrayEquals(damaged, fixture.file(".wallet.pending").readBytes())
    }

    @Test fun corruptCommittedFileDoesNotFallBackToAnOtherwiseValidPendingRecord() = Fixture().use { fixture ->
        fixture.prepare()
        val pending = publicRecord()
        val damaged = byteArrayOf(1)
        fixture.write(".wallet.pending", pending)
        fixture.write("wallet.zcl", damaged)
        authenticatePublicRecord(pending)
        val result = fixture.storage.read()
        assertEquals(CoreStatus.INVALID_ENCODING, result.status)
        assertNull(result.record)
        assertFalse(result.pending)
        assertEquals(CoreStatus.INVALID_ENCODING, fixture.storage.promote(pending))
        assertEquals(CoreStatus.ALREADY_EXISTS, fixture.storage.create(pending))
        assertArrayEquals(damaged, fixture.file("wallet.zcl").readBytes())
        assertArrayEquals(pending, fixture.file(".wallet.pending").readBytes())
    }

    @Test fun truncatedAndOversizedPendingFilesRefuseWithoutChangesOrLeakedDescriptors() = Fixture().use { fixture ->
        fixture.prepare()
        val original = publicRecord()
        assertEquals(124, original.size)
        for (length in listOf(0, 1, 79, 80, 123, 125, 140, 141, 1024)) {
            val damaged = original.copyOf(length)
            fixture.write(".wallet.pending", damaged)
            val result = fixture.storage.read()
            assertEquals(CoreStatus.INVALID_ENCODING, result.status)
            assertNull(result.record)
            assertFalse(result.pending)
            assertEquals(CoreStatus.ALREADY_EXISTS, fixture.storage.create(original))
            assertArrayEquals(damaged, fixture.file(".wallet.pending").readBytes())
            assertFalse(fixture.file("wallet.zcl").exists())
            fixture.noOwnedDescriptors()
        }
    }

    @Test fun finalDirectorySymlinksAndPublicDirectoryPermissionsRefuse() = Fixture().use { fixture ->
        fixture.prepare()
        val alias = File(fixture.root, "alias")
        Files.createSymbolicLink(alias.toPath(), fixture.directory.toPath())
        val linked = WalletStorage(alias.absolutePath)
        val bytes = publicRecord()
        assertEquals(CoreStatus.IO_FAILURE, linked.read().status)
        assertEquals(CoreStatus.IO_FAILURE, linked.create(bytes))
        Os.chmod(fixture.directory.absolutePath, 0x1ed) // 0755 inside this private test root.
        try {
            assertEquals(CoreStatus.IO_FAILURE, fixture.storage.read().status)
            assertEquals(CoreStatus.IO_FAILURE, fixture.storage.create(bytes))
        } finally { Os.chmod(fixture.directory.absolutePath, 0x1c0) }
        assertEquals(CoreStatus.NOT_FOUND, fixture.storage.read().status)
        assertFalse(fixture.file("wallet.zcl").exists())
        assertFalse(fixture.file(".wallet.pending").exists())
    }

    @Test fun recordSymlinksNeverFollowOrOverwriteTheirPublicTarget() = Fixture().use { fixture ->
        fixture.prepare()
        val bytes = publicRecord()
        authenticatePublicRecord(bytes)
        val target = File(fixture.root, "record-target")
        FileOutputStream(target).use { it.write(bytes); it.fd.sync() }
        Os.chmod(target.absolutePath, 0x180)
        for (name in listOf("wallet.zcl", ".wallet.pending")) {
            val linked = fixture.file(name)
            Files.createSymbolicLink(linked.toPath(), target.toPath())
            try {
                assertEquals(CoreStatus.IO_FAILURE, fixture.storage.read().status)
                assertEquals(CoreStatus.ALREADY_EXISTS, fixture.storage.create(bytes))
                assertEquals(CoreStatus.IO_FAILURE, fixture.storage.promote(bytes))
                assertTrue(Files.isSymbolicLink(linked.toPath()))
                assertArrayEquals(bytes, target.readBytes())
                fixture.noOwnedDescriptors()
            } finally { Files.delete(linked.toPath()) }
        }
        assertEquals(CoreStatus.NOT_FOUND, fixture.storage.read().status)
    }

    @Test fun unsafeLockAndRecordModesRefuseAndReleaseDescriptors() = Fixture().use { fixture ->
        fixture.prepare()
        val bytes = publicRecord()
        fixture.write(".lock", byteArrayOf(1))
        assertEquals(CoreStatus.IO_FAILURE, fixture.storage.read().status)
        assertEquals(CoreStatus.IO_FAILURE, fixture.storage.create(bytes))
        assertArrayEquals(byteArrayOf(1), fixture.file(".lock").readBytes())
        fixture.write(".lock", byteArrayOf())
        Os.chmod(fixture.file(".lock").absolutePath, 0x1a4) // 0644: native policy must refuse.
        try { assertEquals(CoreStatus.IO_FAILURE, fixture.storage.read().status) }
        finally { Os.chmod(fixture.file(".lock").absolutePath, 0x180) }
        assertEquals(CoreStatus.OK, fixture.storage.create(bytes))
        authenticatePublicRecord(bytes)
        Os.chmod(fixture.file("wallet.zcl").absolutePath, 0x1a4)
        try {
            assertEquals(CoreStatus.IO_FAILURE, fixture.storage.read().status)
            assertEquals(CoreStatus.IO_FAILURE, fixture.storage.promote(bytes))
            assertEquals(CoreStatus.ALREADY_EXISTS, fixture.storage.create(bytes))
            assertArrayEquals(bytes, fixture.file("wallet.zcl").readBytes())
        } finally { Os.chmod(fixture.file("wallet.zcl").absolutePath, 0x180) }
        assertArrayEquals(bytes, fixture.storage.read().record)
    }

    @Test fun competingAndroidCreatorsHaveExactlyOneCompleteWinner() = Fixture().use { fixture ->
        fixture.prepare()
        val records = List(4) { publicRecord() }
        records.forEach(::authenticatePublicRecord)
        val ready = CountDownLatch(4)
        val release = CountDownLatch(1)
        val workers = Executors.newFixedThreadPool(4)
        try {
            val jobs = records.map { record -> workers.submit<CoreStatus> {
                ready.countDown()
                check(release.await(10, TimeUnit.SECONDS))
                fixture.storage.create(record)
            } }
            assertTrue(ready.await(10, TimeUnit.SECONDS))
            release.countDown()
            val results = jobs.map { it.get(10, TimeUnit.SECONDS) }
            assertEquals(1, results.count { it == CoreStatus.OK })
            assertTrue(results.all { it in setOf(CoreStatus.OK, CoreStatus.BUSY, CoreStatus.ALREADY_EXISTS) })
            val stored = fixture.storage.read()
            assertEquals(CoreStatus.OK, stored.status)
            assertFalse(stored.pending)
            assertArrayEquals(records[results.indexOf(CoreStatus.OK)], stored.record)
            assertFalse(fixture.file(".wallet.pending").exists())
        } finally {
            release.countDown()
            workers.shutdown()
            assertTrue("Fixture creators did not finish", workers.awaitTermination(15, TimeUnit.SECONDS))
        }
    }
}
