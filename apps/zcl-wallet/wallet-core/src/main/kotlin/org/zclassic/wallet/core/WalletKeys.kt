// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet.core

/** Thin JNI adapter. Secret arrays are caller-owned and must be cleared in
 * finally blocks. Do not convert recovery phrases to immutable Strings or log
 * them. Android UI/provider copies cannot all be erased by this adapter.
 * This API implements the documented English BIP39, empty-passphrase profile.
 */
object WalletKeys {
    fun createEntropy(): ByteArray = checkNotNull(NativeCore.createEntropy()) {
        "Secure randomness unavailable"
    }

    fun recoveryPhrase(entropy: ByteArray): CharArray =
        requireNotNull(NativeCore.recoveryPhrase(entropy)) { "Unsupported recovery entropy" }

    fun restoreEntropy(phrase: CharArray): ByteArray =
        requireNotNull(NativeCore.restoreEntropy(phrase)) { "Invalid or unsupported recovery phrase" }

    fun confirmRecoveryPhrase(entropy: ByteArray, phrase: CharArray): Boolean =
        NativeCore.confirmRecoveryPhrase(entropy, phrase)

    fun receivingAddress(entropy: ByteArray, network: Network, index: Int = 0): TransparentAddress {
        val bytes = checkNotNull(NativeCore.receivingAddress(entropy, network.nativeId, index)) {
            "Receiving address derivation failed"
        }
        return TransparentAddress.parse(bytes.toString(Charsets.US_ASCII), network)
    }
}
