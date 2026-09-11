// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet.core

enum class TransparentAddressKind { P2PKH, P2SH }

/** Immutable public view of an address validated by the C core. */
class TransparentAddress private constructor(
    val encoded: String,
    val network: Network,
    private val record: ByteArray,
) {
    val kind: TransparentAddressKind = when (record[0].toInt()) {
        1 -> TransparentAddressKind.P2PKH
        2 -> TransparentAddressKind.P2SH
        else -> error("Invalid native address result")
    }

    fun scriptPubKey(): ByteArray =
        checkNotNull(NativeCore.addressScript(record, network.nativeId)) { "Invalid native address result" }

    override fun equals(other: Any?): Boolean =
        other is TransparentAddress && network == other.network && encoded == other.encoded

    override fun hashCode(): Int = 31 * network.hashCode() + encoded.hashCode()
    override fun toString(): String = encoded

    companion object {
        fun parse(text: String, network: Network): TransparentAddress {
            require(text.length <= 35) { "Invalid address text size" }
            val record = requireNotNull(NativeCore.parseAddress(text.toByteArray(Charsets.UTF_8), network.nativeId)) {
                "Invalid or unsupported transparent address"
            }
            check(record.size == 21) { "Invalid native address result" }
            return TransparentAddress(text, network, record)
        }

        fun fromPublicKeyHash(hash: ByteArray, network: Network): TransparentAddress {
            val encoded = requireNotNull(NativeCore.addressFromHash(hash, network.nativeId)) { "Invalid public hash" }
            return parse(encoded.toString(Charsets.US_ASCII), network)
        }
    }
}
