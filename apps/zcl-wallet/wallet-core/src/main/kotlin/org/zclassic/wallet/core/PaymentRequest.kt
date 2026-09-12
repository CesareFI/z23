// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet.core

import java.nio.ByteBuffer
import java.nio.ByteOrder

/** Public display model. A parsed request never authorizes payment. */
data class PaymentRequest(
    val address: TransparentAddress,
    val amount: Zatoshi? = null,
    val label: String? = null,
    val message: String? = null,
) {
    companion object {
        fun parse(text: String, network: Network): PaymentRequest {
            require(text.length <= 1024) { "Invalid payment request size" }
            val record = requireNotNull(NativeCore.parsePayment(text.toByteArray(Charsets.UTF_8), network.nativeId)) {
                "Invalid or unsupported Zclassic payment request"
            }
            return fromNative(record, network)
        }

        // Decode the bounded JNI adapter record, not a blockchain/URI format.
        internal fun fromNative(record: ByteArray, network: Network): PaymentRequest {
            check(record.size in 49..449 && record[0] == 1.toByte()) { "Invalid native payment result" }
            val flags = record[1].toInt()
            val buffer = ByteBuffer.wrap(record).order(ByteOrder.LITTLE_ENDIAN)
            val amount = buffer.getLong(37)
            val labelSize = buffer.getShort(45).toInt()
            val messageSize = buffer.getShort(47).toInt()
            check(flags in 0..7 && labelSize in 0..200 && messageSize in 0..200) { "Invalid native payment result" }
            check(record.size == 49 + labelSize + messageSize) { "Invalid native payment result" }
            val address = record.copyOfRange(2, 37).toString(Charsets.US_ASCII)
            val label = record.copyOfRange(49, 49 + labelSize).toString(Charsets.UTF_8)
            val message = record.copyOfRange(49 + labelSize, record.size).toString(Charsets.UTF_8)
            return PaymentRequest(
                TransparentAddress.parse(address, network),
                if (flags and 1 != 0) Zatoshi.of(amount) else null,
                if (flags and 2 != 0) label else null,
                if (flags and 4 != 0) message else null,
            )
        }
    }
}
