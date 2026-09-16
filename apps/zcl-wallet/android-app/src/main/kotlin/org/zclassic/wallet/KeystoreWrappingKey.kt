// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.app.KeyguardManager
import android.content.Context
import android.hardware.biometrics.BiometricManager
import android.os.Build
import android.security.keystore.KeyGenParameterSpec
import android.security.keystore.KeyInfo
import android.security.keystore.KeyProperties
import java.security.KeyStore
import javax.crypto.Cipher
import javax.crypto.KeyGenerator
import javax.crypto.SecretKey
import javax.crypto.SecretKeyFactory
import javax.crypto.spec.GCMParameterSpec
import org.zclassic.wallet.core.CoreStatus
import org.zclassic.wallet.core.WalletRecord
import org.zclassic.wallet.core.WalletStorage
import org.zclassic.wallet.core.WrappingPolicy

/** Android-only nonexportable wrapping-key adapter. C owns record/storage and
 * the normalized custody acceptance policy. Call off the UI thread. A cipher
 * must be authenticated through BiometricPrompt before its single doFinal.
 * There is no deletion, key replacement, timed-auth or software fallback path.
 */
internal class KeystoreWrappingKey(context: Context, private val storage: WalletStorage) {
    private val context = context.applicationContext

    fun prepareCreation(): Cipher = synchronized(aliasLock) {
        requireUnlockedDevice()
        check(storage.read().status == CoreStatus.NOT_FOUND) { "Existing wallet data requires recovery" }
        val store = openStore()
        val key = if (store.containsAlias(ALIAS)) existingKey(store) else generateKey()
        requirePolicy(key)
        Cipher.getInstance(TRANSFORMATION).apply { init(Cipher.ENCRYPT_MODE, key) }
    }

    fun prepareUnlock(record: WalletRecord): Cipher = synchronized(aliasLock) {
        requireUnlockedDevice()
        // A missing/invalidated alias never causes key generation for a record.
        val key = existingKey(openStore())
        requirePolicy(key)
        Cipher.getInstance(TRANSFORMATION).apply {
            init(Cipher.DECRYPT_MODE, key, GCMParameterSpec(128, record.iv))
        }
    }

    private fun requireUnlockedDevice() {
        val keyguard = checkNotNull(context.getSystemService(KeyguardManager::class.java))
        check(keyguard.isDeviceSecure && !keyguard.isDeviceLocked) { "Unlock a device with a secure screen lock" }
        val biometrics = checkNotNull(context.getSystemService(BiometricManager::class.java))
        val allowed = BiometricManager.Authenticators.BIOMETRIC_STRONG or
            BiometricManager.Authenticators.DEVICE_CREDENTIAL
        check(biometrics.canAuthenticate(allowed) == BiometricManager.BIOMETRIC_SUCCESS) {
            "Required device authentication is unavailable"
        }
    }

    private fun generateKey(): SecretKey {
        val spec = KeyGenParameterSpec.Builder(ALIAS, PURPOSES)
            .setKeySize(256)
            .setBlockModes(KeyProperties.BLOCK_MODE_GCM)
            .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
            .setRandomizedEncryptionRequired(true)
            .setUserAuthenticationRequired(true)
            .setUserAuthenticationParameters(0, AUTHENTICATION)
        // Android's documented 12..14 bugs can prevent legitimate use after
        // non-strong biometric unlock. Per-use authentication is always required.
        if (Build.VERSION.SDK_INT >= 35) spec.setUnlockedDeviceRequired(true)
        return KeyGenerator.getInstance(KeyProperties.KEY_ALGORITHM_AES, PROVIDER).apply {
            init(spec.build())
        }.generateKey()
    }

    private fun existingKey(store: KeyStore): SecretKey =
        checkNotNull(store.getKey(ALIAS, null) as? SecretKey) { "Wallet wrapping key is unavailable" }

    companion object {
        private const val ALIAS = "org.zclassic.wallet.wrap.v1"
        private const val PROVIDER = "AndroidKeyStore"
        private const val TRANSFORMATION = "AES/GCM/NoPadding"
        private const val PURPOSES = KeyProperties.PURPOSE_ENCRYPT or KeyProperties.PURPOSE_DECRYPT
        private const val AUTHENTICATION = KeyProperties.AUTH_BIOMETRIC_STRONG or KeyProperties.AUTH_DEVICE_CREDENTIAL
        // Wallet work stays in the UI process; the isolated QR process has a
        // separate UID. Serialize activities; C independently refuses overwrite.
        private val aliasLock = Any()

        private fun openStore(): KeyStore = KeyStore.getInstance(PROVIDER).apply { load(null) }

        internal fun requirePolicy(key: SecretKey) {
            val info = SecretKeyFactory.getInstance(key.algorithm, PROVIDER)
                .getKeySpec(key, KeyInfo::class.java) as KeyInfo
            check(WrappingPolicy.accepts(info.keySize, hardware(info), flags(key, info),
                info.userAuthenticationValidityDurationSeconds, methods(info))) {
                "Device does not provide the required wallet key protection"
            }
        }

        private fun aesGcmOnly(key: SecretKey, info: KeyInfo): Boolean =
            key.algorithm == KeyProperties.KEY_ALGORITHM_AES && info.purposes == PURPOSES &&
                info.blockModes.contentEquals(arrayOf(KeyProperties.BLOCK_MODE_GCM)) &&
                info.encryptionPaddings.contentEquals(arrayOf(KeyProperties.ENCRYPTION_PADDING_NONE))

        private fun flags(key: SecretKey, info: KeyInfo): Int {
            var value = 0
            if (aesGcmOnly(key, info)) value = value or WrappingPolicy.AES_GCM_ONLY
            if (info.origin == KeyProperties.ORIGIN_GENERATED) value = value or WrappingPolicy.GENERATED
            if (info.isUserAuthenticationRequired) value = value or WrappingPolicy.AUTH_REQUIRED
            if (info.isUserAuthenticationRequirementEnforcedBySecureHardware) value = value or WrappingPolicy.HARDWARE_AUTH
            if (info.isUserAuthenticationValidWhileOnBody) value = value or WrappingPolicy.ON_BODY
            return value
        }

        private fun methods(info: KeyInfo): Int {
            // Reject unknown provider bits instead of masking them away.
            if (info.userAuthenticationType != AUTHENTICATION) return 0
            return WrappingPolicy.STRONG_BIOMETRIC or WrappingPolicy.DEVICE_CREDENTIAL
        }

        @Suppress("DEPRECATION") // API 30 has only the older hardware query.
        private fun hardware(info: KeyInfo): Int {
            if (Build.VERSION.SDK_INT < 31) return if (info.isInsideSecureHardware) WrappingPolicy.TEE else 0
            return when (info.securityLevel) {
                KeyProperties.SECURITY_LEVEL_TRUSTED_ENVIRONMENT -> WrappingPolicy.TEE
                KeyProperties.SECURITY_LEVEL_STRONGBOX -> WrappingPolicy.STRONGBOX
                else -> 0
            }
        }
    }
}
