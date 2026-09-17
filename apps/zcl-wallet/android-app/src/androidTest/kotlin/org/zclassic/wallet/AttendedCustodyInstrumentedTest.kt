// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.app.Activity
import android.os.Build
import android.os.Bundle
import android.os.Process
import android.os.SystemClock
import android.view.View
import android.view.WindowManager
import android.widget.TextView
import android.widget.RadioButton
import androidx.test.core.app.ActivityScenario
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import java.io.File
import java.security.GeneralSecurityException
import java.security.KeyStore
import java.security.ProviderException
import java.util.concurrent.atomic.AtomicReference
import javax.crypto.Cipher
import javax.crypto.SecretKey
import javax.crypto.spec.GCMParameterSpec
import org.junit.Assert.*
import org.junit.Assume.assumeTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.zclassic.wallet.core.CoreStatus
import org.zclassic.wallet.core.WalletRecord
import org.zclassic.wallet.core.WalletStorage

/** Attended, one-shot public-vector acceptance in the separate qualification
 * UID. Never inject/read credentials, capture screens, delete keys/files or
 * accept a software-key fallback. Retain the resulting fixture for inspection.
 * Build metadata admission is not physical attestation or hostile-OS defense. */
@RunWith(AndroidJUnit4::class)
class AttendedCustodyInstrumentedTest {
    private val instrumentation = InstrumentationRegistry.getInstrumentation()
    private var activity: MainActivity? = null

    private fun store() = KeyStore.getInstance("AndroidKeyStore").apply { load(null) }

    private fun onUi(action: (Activity) -> Unit) {
        val failure = AtomicReference<Throwable?>()
        instrumentation.runOnMainSync {
            try { action(checkNotNull(activity)) } catch (problem: Throwable) { failure.set(problem) }
        }
        failure.get()?.let { throw it }
    }

    private fun awaitView(identifier: Int) {
        val start = SystemClock.elapsedRealtime()
        check(start >= 0)
        while (true) {
            val now = SystemClock.elapsedRealtime()
            check(now >= start && now - start < 75_000) { "Attended custody stage did not complete in time" }
            var visible = false
            onUi {
                check(it.findViewById<View>(R.id.retry_wallet)?.isShown != true) {
                    "Wallet refused the attended custody operation"
                }
                visible = it.findViewById<View>(identifier)?.isShown == true
            }
            if (visible) return
            Thread.sleep(50)
        }
    }

    private fun click(identifier: Int) {
        awaitView(identifier)
        onUi { check(checkNotNull(it.findViewById<View>(identifier)).performClick()) }
    }

    private fun stage(name: String) {
        instrumentation.sendStatus(2, Bundle().apply { putString("public_custody_stage", name) })
    }

    private fun requireExpectedAddress() {
        awaitView(R.id.receiving_address)
        onUi {
            // 128 zero entropy bits, empty passphrase, testnet m/44'/1'/0'/0/0.
            // Independently projected by the existing host OpenSSL oracle.
            assertEquals("tmF1xjfhsSzhy55dmhorzTnKjtHhZmPKzts",
                it.findViewById<TextView>(R.id.receiving_address).text.toString())
            assertTrue(it.window.attributes.flags and WindowManager.LayoutParams.FLAG_SECURE != 0)
        }
    }

    private fun restorePublicVector() {
        awaitView(R.id.network_testnet)
        onUi { assertTrue(it.findViewById<RadioButton>(R.id.network_testnet).isChecked) }
        stage("restore-authentication-required")
        click(R.id.restore_wallet)
        // Only the device owner interacts with the system authentication UI.
        awaitView(R.id.recovery_input)
        val phrase = "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon about".toCharArray()
        try {
            onUi {
                val input = checkNotNull(it.findViewById<RecoveryInputView>(R.id.recovery_input))
                for (character in phrase) input.append(character)
            }
            click(R.id.save_wallet)
        } finally { phrase.fill('\u0000') }
        requireExpectedAddress()
    }

    private fun requireAnotherAuthentication(record: WalletRecord) {
        val key = checkNotNull(store().getKey(CustodyQualificationBoundary.ALIAS, null) as? SecretKey)
        KeystoreWrappingKey.requirePolicy(key)
        val output = ByteArray(32)
        var refused = false
        try {
            val cipher = Cipher.getInstance("AES/GCM/NoPadding")
            cipher.init(Cipher.DECRYPT_MODE, key, GCMParameterSpec(128, record.iv))
            cipher.updateAAD(record.header)
            cipher.doFinal(record.ciphertext, 0, record.ciphertext.size, output, 0)
        } catch (problem: GeneralSecurityException) {
            refused = custodyAuthenticationRefused(problem)
        } catch (problem: ProviderException) {
            refused = custodyAuthenticationRefused(problem)
        } finally { output.fill(0) }
        assertTrue("Another operation did not require its own authentication", refused)
    }

    private fun committed(storage: WalletStorage, directory: File): ByteArray {
        val read = storage.read()
        assertEquals(CoreStatus.OK, read.status)
        assertFalse(read.pending)
        assertFalse("Restore initialized historical change state", File(directory, ".change.index").exists())
        return checkNotNull(read.record)
    }

    @Test fun restoreAndTwoIndependentAuthenticatedUnlocks() {
        val arguments = InstrumentationRegistry.getArguments()
        val consent = arguments.getString("custodyHardware")
        assumeTrue("Attended public-vector custody requires explicit opt-in",
            consent != null)
        val context = instrumentation.targetContext
        val info = context.applicationInfo
        CustodyQualificationBoundary.requireIdentity(CustodyQualificationBoundary.Identity(
            context.packageName, info.uid, Process.myUid(), info.flags, Build.HARDWARE, Build.PRODUCT),
            consent, arguments.getString("qualificationUid"))
        // No private-path or Keystore inspection occurs until identity admission.
        val directory = File(context.noBackupFilesDir, "wallet-v1")
        CustodyQualificationBoundary.requireFresh(directory.toPath()) {
            store().containsAlias(CustodyQualificationBoundary.ALIAS)
        }
        val storage = WalletStorage(directory.absolutePath)
        try {
            ActivityScenario.launch(MainActivity::class.java).use { scenario ->
                scenario.onActivity { activity = it }
                restorePublicVector()
                val original = committed(storage, directory)
                val record = WalletRecord.parse(original)
                requireAnotherAuthentication(record)
                repeat(2) { index ->
                    click(R.id.lock_wallet)
                    stage("unlock-${index + 1}-authentication-required")
                    click(R.id.unlock_wallet)
                    requireExpectedAddress()
                    assertArrayEquals(original, committed(storage, directory))
                    requireAnotherAuthentication(record)
                }
                stage("restore-and-two-unlocks-complete")
            }
        } finally { activity = null }
        // Deliberately retain this published-vector wallet/key. A later run
        // refuses them, and no cleanup can delete material from another run.
    }
}
