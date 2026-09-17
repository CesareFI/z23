// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.content.pm.ApplicationInfo
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import java.io.IOException
import java.nio.file.Files
import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith

/** Synthetic metadata and invocation-owned cache paths only. A positive
 * predicate case does not claim the host is physical or policy-qualified. */
@RunWith(AndroidJUnit4::class)
class CustodyQualificationBoundaryInstrumentedTest {
    private val identity = CustodyQualificationBoundary.Identity(
        CustodyQualificationBoundary.PACKAGE, 12345, 12345,
        ApplicationInfo.FLAG_TEST_ONLY or ApplicationInfo.FLAG_DEBUGGABLE,
        "public-hardware-fixture", "public-product-fixture")

    private fun admit(value: CustodyQualificationBoundary.Identity = identity,
                      consent: String? = CustodyQualificationBoundary.CONSENT, uid: String? = "12345") =
        CustodyQualificationBoundary.requireIdentity(value, consent, uid)

    @Test fun onlyTheExactOptedInQualificationIdentityPasses() {
        admit()
        for (consent in listOf(null, "", "yes", "restore-public-vector "))
            assertThrows(IllegalStateException::class.java) { admit(consent = consent) }
        for (uid in listOf(null, "", "012345", "12346", "-1", "2147483648"))
            assertThrows(IllegalStateException::class.java) { admit(uid = uid) }
    }

    @Test fun normalPackagesMissingFlagsAndUidMismatchRefuse() {
        val invalid = listOf(identity.copy(packageName = "org.zclassic.wallet.dev"),
            identity.copy(packageName = identity.packageName + ".test"), identity.copy(uid = 0),
            identity.copy(processUid = 12346), identity.copy(flags = ApplicationInfo.FLAG_TEST_ONLY),
            identity.copy(flags = ApplicationInfo.FLAG_DEBUGGABLE), identity.copy(flags = 0))
        for (value in invalid) assertThrows(IllegalStateException::class.java) { admit(value) }
    }

    @Test fun knownEmulatorsAndMissingMetadataRefuse() {
        for (hardware in listOf("ranchu", "RANCHU", "goldfish", ""))
            assertThrows(IllegalStateException::class.java) { admit(identity.copy(hardware = hardware)) }
        for (product in listOf("sdk_gphone64_x86_64", "emu64xa", "SDK_PHONE", ""))
            assertThrows(IllegalStateException::class.java) { admit(identity.copy(product = product)) }
    }

    @Test fun existingFilesDirectoriesAndDanglingLinksRemainUntouched() {
        val context = InstrumentationRegistry.getInstrumentation().targetContext
        val parent = Files.createTempDirectory(context.cacheDir.toPath(), "public-custody-boundary-")
        val path = parent.resolve("candidate")
        var queries = 0
        val key: () -> Boolean = { queries++; false }
        try {
            CustodyQualificationBoundary.requireFresh(path, key)
            assertEquals(1, queries)
            assertFalse(Files.exists(path))
            Files.createDirectory(path)
            assertThrows(IllegalStateException::class.java) { CustodyQualificationBoundary.requireFresh(path, key) }
            assertTrue(Files.isDirectory(path))
            Files.delete(path)
            Files.write(path, byteArrayOf(0x61))
            assertThrows(IllegalStateException::class.java) { CustodyQualificationBoundary.requireFresh(path, key) }
            assertArrayEquals(byteArrayOf(0x61), Files.readAllBytes(path))
            Files.delete(path)
            Files.createSymbolicLink(path, parent.resolve("absent"))
            assertThrows(IllegalStateException::class.java) { CustodyQualificationBoundary.requireFresh(path, key) }
            assertTrue(Files.isSymbolicLink(path))
            assertEquals(1, queries) // Refusal precedes any key query.
        } finally {
            Files.deleteIfExists(path) // Only this test's fixed leaf, including its link.
            Files.delete(parent)
        }
    }

    @Test fun existingKeysAndInspectionFailureRefuseWithoutCreatingFiles() {
        val context = InstrumentationRegistry.getInstrumentation().targetContext
        val parent = Files.createTempDirectory(context.cacheDir.toPath(), "public-custody-key-boundary-")
        val path = parent.resolve("candidate")
        try {
            assertThrows(IllegalStateException::class.java) {
                CustodyQualificationBoundary.requireFresh(path) { true }
            }
            val failure = IOException("Public fixture key-query failure")
            assertSame(failure, assertThrows(IOException::class.java) {
                CustodyQualificationBoundary.requireFresh(path) { throw failure }
            })
            assertFalse(Files.exists(path))
        } finally { Files.delete(parent) }
    }

    @Test fun absentFileOrSymlinkParentsCannotMasqueradeAsFreshStorage() {
        val context = InstrumentationRegistry.getInstrumentation().targetContext
        val root = Files.createTempDirectory(context.cacheDir.toPath(), "public-custody-parent-")
        val parent = root.resolve("parent")
        var queried = false
        val key: () -> Boolean = { queried = true; false }
        try {
            assertThrows(IOException::class.java) {
                CustodyQualificationBoundary.requireFresh(parent.resolve("wallet"), key)
            }
            Files.createSymbolicLink(parent, root)
            assertThrows(IllegalStateException::class.java) {
                CustodyQualificationBoundary.requireFresh(parent.resolve("wallet"), key)
            }
            assertTrue(Files.isSymbolicLink(parent))
            Files.delete(parent)
            Files.createFile(parent)
            assertThrows(IllegalStateException::class.java) {
                CustodyQualificationBoundary.requireFresh(parent.resolve("wallet"), key)
            }
            assertTrue(Files.isRegularFile(parent))
            assertFalse(queried)
        } finally {
            Files.deleteIfExists(parent)
            Files.delete(root)
        }
    }
}
