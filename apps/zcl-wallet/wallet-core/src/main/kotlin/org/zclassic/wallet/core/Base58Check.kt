// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet.core

/** Public-data adapter to the bounded C codec. Not a secret-key interface. */
object Base58Check {
    fun encode(payload: ByteArray): String =
        requireNotNull(NativeCore.encodeBase58(payload)) { "Invalid Base58 payload" }
            .toString(Charsets.US_ASCII)

    fun decode(text: String): ByteArray {
        require(text.length <= 184) { "Invalid Base58 text size" }
        return requireNotNull(NativeCore.decodeBase58(text.toByteArray(Charsets.UTF_8))) { "Invalid Base58 data" }
    }
}
