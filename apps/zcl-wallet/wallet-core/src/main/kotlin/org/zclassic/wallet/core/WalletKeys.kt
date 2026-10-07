// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet.core

/** Thin JNI adapter. Secret arrays are caller-owned and must be cleared in
 * finally blocks. Do not convert recovery phrases to immutable Strings or log
 * them. Android UI/provider copies cannot all be erased by this adapter.
 * This API implements the documented English BIP39, empty-passphrase profile.
 */
object WalletKeys {
    /** The creation profile is 128-bit BIP39 entropy (twelve English words). */
    fun createEntropy(): ByteArray = SecretOutput.bytes(16) { output ->
        NativeCore.createEntropy(output).also { check(it == 16) { "Secure randomness unavailable" } }
    }

    fun recoveryPhrase(entropy: ByteArray): CharArray = SecretOutput.characters(215) { output ->
        NativeCore.recoveryPhrase(entropy, output).also { require(it > 0) { "Unsupported recovery entropy" } }
    }

    fun restoreEntropy(phrase: CharArray): ByteArray = SecretOutput.bytes(32) { output ->
        NativeCore.restoreEntropy(phrase, output).also {
            require(it > 0) { "Invalid or unsupported recovery phrase" }
        }
    }

    fun confirmRecoveryPhrase(entropy: ByteArray, phrase: CharArray): Boolean =
        NativeCore.confirmRecoveryPhrase(entropy, phrase)

    fun receivingAddress(entropy: ByteArray, network: Network, index: Int = 0): TransparentAddress {
        val bytes = checkNotNull(NativeCore.receivingAddress(entropy, network.nativeId, index)) {
            "Receiving address derivation failed"
        }
        return TransparentAddress.parse(bytes.toString(Charsets.US_ASCII), network)
    }
}
