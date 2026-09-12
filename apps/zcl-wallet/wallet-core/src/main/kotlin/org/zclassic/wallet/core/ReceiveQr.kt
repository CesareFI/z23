// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet.core

/** Immutable public rendering data. All QR encoding and address validation is C. */
class ReceiveQr private constructor(private val record: ByteArray) {
    val side: Int = record[0].toInt()

    fun isDark(x: Int, y: Int): Boolean {
        require(x in 0 until side && y in 0 until side) { "Invalid QR coordinate" }
        return record[1 + y * side + x] == 1.toByte()
    }

    companion object {
        fun forAddress(address: TransparentAddress): ReceiveQr {
            val result = checkNotNull(NativeCore.receiveQr(
                address.encoded.toByteArray(Charsets.US_ASCII), address.network.nativeId)) {
                "Receiving QR unavailable"
            }
            check(result.isNotEmpty()) { "Invalid native QR result" }
            val side = result[0].toInt()
            check(side in 29..41 && side % 4 == 1 && result.size == 1 + side * side) {
                "Invalid native QR dimensions"
            }
            check(result.drop(1).all { it == 0.toByte() || it == 1.toByte() }) { "Invalid native QR module" }
            return ReceiveQr(result)
        }
    }
}
