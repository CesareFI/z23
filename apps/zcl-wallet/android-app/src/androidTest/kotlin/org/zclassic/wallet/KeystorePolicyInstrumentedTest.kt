// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.app.KeyguardManager
import android.os.Build
import android.os.Bundle
import android.security.keystore.KeyGenParameterSpec
import android.security.keystore.KeyInfo
import android.security.keystore.KeyProperties
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import java.security.KeyStore
import java.util.UUID
import javax.crypto.Cipher
import javax.crypto.KeyGenerator
import javax.crypto.SecretKey
import javax.crypto.SecretKeyFactory
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertThrows
import org.junit.Assert.assertTrue
import org.junit.Assume.assumeTrue
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class KeystorePolicyInstrumentedTest {
    private fun withTestKey(authenticated: Boolean, test: (SecretKey) -> Unit) {
        val alias = "org.zclassic.wallet.instrumentation." + UUID.randomUUID().toString()
        val store = KeyStore.getInstance("AndroidKeyStore").apply { load(null) }
        assertFalse(store.containsAlias(alias))
        try {
            val builder = KeyGenParameterSpec.Builder(alias,
                KeyProperties.PURPOSE_ENCRYPT or KeyProperties.PURPOSE_DECRYPT)
                .setKeySize(256)
                .setBlockModes(KeyProperties.BLOCK_MODE_GCM)
                .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
                .setUserAuthenticationRequired(authenticated)
            if (authenticated) builder.setUserAuthenticationParameters(0,
                KeyProperties.AUTH_BIOMETRIC_STRONG or KeyProperties.AUTH_DEVICE_CREDENTIAL)
            val key = KeyGenerator.getInstance("AES", "AndroidKeyStore").apply {
                init(builder.build())
            }.generateKey()
            test(key)
        } finally {
            // Only this invocation's newly generated test alias, never the
            // application's wrapping-key alias or an operator's key.
            if (store.containsAlias(alias)) store.deleteEntry(alias)
            assertFalse(store.containsAlias(alias))
        }
    }

    @Test fun generatedUnauthenticatedKeyIsRejectedByTheCPolicy() = withTestKey(false) { key ->
        assertThrows(IllegalStateException::class.java) { KeystoreWrappingKey.requirePolicy(key) }
    }

    @Test fun providerPerUseMetadataAndUnauthenticatedCipherRefusal() {
        val context = InstrumentationRegistry.getInstrumentation().targetContext
        val keyguard = checkNotNull(context.getSystemService(KeyguardManager::class.java))
        assumeTrue("Isolated device needs a test screen lock", keyguard.isDeviceSecure)
        withTestKey(true) { key ->
            val info = SecretKeyFactory.getInstance("AES", "AndroidKeyStore")
                .getKeySpec(key, KeyInfo::class.java) as KeyInfo
            assertTrue(info.isUserAuthenticationRequired)
            assertTrue(info.userAuthenticationValidityDurationSeconds in -1..0)
            assertEquals(KeyProperties.AUTH_BIOMETRIC_STRONG or KeyProperties.AUTH_DEVICE_CREDENTIAL,
                info.userAuthenticationType)
            val policyAccepted = runCatching { KeystoreWrappingKey.requirePolicy(key) }.isSuccess
            val facts = Bundle().apply {
                putInt("key_bits", info.keySize)
                if (Build.VERSION.SDK_INT >= 31) putInt("security_level", info.securityLevel)
                putBoolean("hardware_auth", info.isUserAuthenticationRequirementEnforcedBySecureHardware)
                putInt("authentication_seconds", info.userAuthenticationValidityDurationSeconds)
                putInt("authentication_methods", info.userAuthenticationType)
                putBoolean("c_policy_accepted", policyAccepted)
            }
            // Virtualized test hardware can report a TEE. Only assert the
            // provider's behavior here, not physical hardware authenticity.
            val cipher = Cipher.getInstance("AES/GCM/NoPadding")
            val fixture = ByteArray(16) // Public, unfunded test data.
            var refused = false
            var stage = "initialize"
            try {
                cipher.init(Cipher.ENCRYPT_MODE, key)
                stage = "finalize"
                cipher.doFinal(fixture)
            } catch (error: java.security.GeneralSecurityException) {
                refused = custodyAuthenticationRefused(error)
            } catch (error: java.security.ProviderException) {
                refused = custodyAuthenticationRefused(error)
            } finally {
                fixture.fill(0)
            }
            facts.putString("unauthenticated_refusal_stage", stage)
            // Public provider capabilities only: no alias, key, IV or fixture bytes.
            InstrumentationRegistry.getInstrumentation().sendStatus(2, facts)
            assertTrue("Per-use operation must require its own authentication", refused)
        }
    }
}
