// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet.core

/** Bounded JNI adapter. Wallet decisions and validation live in portable C. */
internal object NativeCore {
    init { System.loadLibrary("zclwallet_jni") }

    @JvmStatic external fun parseAmount(text: ByteArray): Long
    @JvmStatic external fun formatAmount(amount: Long): ByteArray?
    @JvmStatic external fun changeAmount(left: Long, right: Long, subtract: Boolean): Long
    @JvmStatic external fun encodeBase58(payload: ByteArray): ByteArray?
    @JvmStatic external fun decodeBase58(text: ByteArray): ByteArray?
    @JvmStatic external fun parseAddress(text: ByteArray, network: Int): ByteArray?
    @JvmStatic external fun addressScript(record: ByteArray, network: Int): ByteArray?
    @JvmStatic external fun addressFromHash(hash: ByteArray, network: Int): ByteArray?
    @JvmStatic external fun parsePayment(text: ByteArray, network: Int): ByteArray?
}
