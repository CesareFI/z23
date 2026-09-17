// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.content.pm.ApplicationInfo
import java.nio.file.Files
import java.nio.file.LinkOption
import java.nio.file.NoSuchFileException
import java.nio.file.Path
import java.nio.file.attribute.BasicFileAttributes
import java.util.Locale

/** Test-APK admission only. No directory/key creation, deletion or app policy
 * override. Build fields reject known emulators; they are not attestation. */
internal object CustodyQualificationBoundary {
    const val PACKAGE = "org.zclassic.wallet.dev.qualification"
    const val CONSENT = "restore-public-vector"
    const val ALIAS = "org.zclassic.wallet.wrap.v1"

    data class Identity(val packageName: String, val uid: Int, val processUid: Int,
                        val flags: Int, val hardware: String, val product: String)

    fun requireIdentity(identity: Identity, consent: String?, approvedUid: String?) {
        check(consent == CONSENT) { "Explicit public-vector custody opt-in is required" }
        check(identity.packageName == PACKAGE) { "Only the qualification package is permitted" }
        val flags = ApplicationInfo.FLAG_TEST_ONLY or ApplicationInfo.FLAG_DEBUGGABLE
        check(identity.flags and flags == flags) { "Qualification must be test-only and debuggable" }
        check(identity.uid > 0 && identity.uid == identity.processUid &&
            approvedUid == identity.uid.toString()) { "Explicit qualification UID does not match" }
        val hardware = identity.hardware.lowercase(Locale.ROOT)
        val product = identity.product.lowercase(Locale.ROOT)
        check(hardware.isNotBlank() && product.isNotBlank()) { "Missing device identity" }
        check(hardware != "ranchu" && hardware != "goldfish" &&
            !product.startsWith("sdk") && !product.startsWith("emu")) {
            "Attended hardware fixture refuses known emulators"
        }
    }

    fun requireFresh(directory: Path, aliasExists: () -> Boolean) {
        val parent = checkNotNull(directory.parent) { "Qualification requires an owned parent directory" }
        val parentInfo = Files.readAttributes(parent, BasicFileAttributes::class.java, LinkOption.NOFOLLOW_LINKS)
        check(parentInfo.isDirectory) { "Qualification parent is not a real directory" }
        // Files.exists can hide an inspection error as false. Only an explicit
        // absent path permits admission; a symlink or empty scaffold refuses.
        val absent = try {
            Files.readAttributes(directory, BasicFileAttributes::class.java, LinkOption.NOFOLLOW_LINKS)
            false
        } catch (_: NoSuchFileException) { true }
        check(absent) { "Qualification wallet path already exists" }
        check(!aliasExists()) { "Qualification wrapping key already exists" }
    }
}
