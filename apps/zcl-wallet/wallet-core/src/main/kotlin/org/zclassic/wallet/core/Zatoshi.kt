// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet.core

/** Nonnegative ZCL money. Construction and arithmetic always enforce MoneyRange. */
@JvmInline
value class Zatoshi private constructor(val value: Long) : Comparable<Zatoshi> {
    operator fun plus(other: Zatoshi): Zatoshi = fromNative(NativeCore.changeAmount(value, other.value, false))
    operator fun minus(other: Zatoshi): Zatoshi = fromNative(NativeCore.changeAmount(value, other.value, true))
    override fun compareTo(other: Zatoshi): Int = value.compareTo(other.value)

    fun format(): String {
        val bytes = requireNotNull(NativeCore.formatAmount(value)) { "Invalid ZCL amount" }
        return bytes.toString(Charsets.US_ASCII)
    }

    companion object {
        const val PER_ZCL = 100_000_000L
        const val MAX_VALUE = 21_000_000L * PER_ZCL
        val ZERO = Zatoshi(0)

        fun of(value: Long): Zatoshi {
            return fromNative(NativeCore.changeAmount(value, 0, false))
        }

        fun parse(text: String): Zatoshi {
            // Cap managed encoding allocation before JNI's independent byte cap.
            require(text.length <= 17) { "Invalid ZCL amount" }
            return fromNative(NativeCore.parseAmount(text.toByteArray(Charsets.UTF_8)))
        }

        private fun fromNative(result: Long): Zatoshi {
            require(result >= 0) { "Invalid ZCL amount" }
            return Zatoshi(result)
        }
    }
}
