// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet.core

/** Own the destination before JNI can write secret bytes. A pending native
 * exception returns through this finally, retaining an owner for partial output.
 * This clears our arrays; it cannot erase every VM/provider/UI copy. */
internal object SecretOutput {
    inline fun bytes(capacity: Int, write: (ByteArray) -> Int): ByteArray {
        require(capacity in 1..32)
        val output = ByteArray(capacity)
        var transferred = false
        try {
            val length = write(output)
            check(length in 1..capacity) { "Invalid native secret output length" }
            if (length != capacity) return output.copyOf(length)
            transferred = true
            return output
        } finally {
            if (!transferred) output.fill(0)
        }
    }

    inline fun characters(capacity: Int, write: (CharArray) -> Int): CharArray {
        require(capacity in 1..215)
        val output = CharArray(capacity)
        var transferred = false
        try {
            val length = write(output)
            check(length in 1..capacity) { "Invalid native secret output length" }
            if (length != capacity) return output.copyOf(length)
            transferred = true
            return output
        } finally {
            if (!transferred) output.fill('\u0000')
        }
    }
}
