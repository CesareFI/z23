// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet.core

/** Structurally checked, UNAUTHENTICATED storage data. The Android provider
 * must authenticate [header] as GCM AAD and decrypt [ciphertext] before using
 * the recovered entropy. Parsing alone never authorizes an address or balance.
 * Arrays are caller-owned and must not be shared with concurrent writers.
 */
class WalletRecord private constructor(
    val header: ByteArray,
    val iv: ByteArray,
    val ciphertext: ByteArray,
    val network: Network,
) {
    companion object {
        fun createHeader(entropy: ByteArray, network: Network): ByteArray =
            checkNotNull(NativeCore.createWalletHeader(entropy, network.nativeId)) {
                "Wallet header derivation failed"
            }

        fun pack(header: ByteArray, iv: ByteArray, ciphertext: ByteArray): ByteArray =
            requireNotNull(NativeCore.packWalletRecord(header, iv, ciphertext)) {
                "Invalid encrypted wallet record"
            }

        fun parse(bytes: ByteArray): WalletRecord {
            val parts = requireNotNull(NativeCore.unpackWalletRecord(bytes)) {
                "Invalid or unsupported wallet record"
            }
            check(parts.size == 4 && parts[3].size == 1) { "Invalid native record result" }
            val network = Network.entries.single { it.nativeId == parts[3][0].toInt() }
            return WalletRecord(parts[0], parts[1], parts[2], network)
        }

        /** Call only after successful platform GCM authentication. Entropy is
         * caller-owned: clear it in finally, including derivation failure. */
        fun recoveredAddress(authenticatedHeader: ByteArray, entropy: ByteArray, network: Network): TransparentAddress {
            val address = requireNotNull(NativeCore.recoveredWalletAddress(authenticatedHeader, entropy)) {
                "Recovered wallet does not match its authenticated header"
            }
            return TransparentAddress.parse(address.toString(Charsets.US_ASCII), network)
        }
    }
}
