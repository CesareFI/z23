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
    @JvmStatic external fun receiveQr(text: ByteArray, network: Int): ByteArray?
    @JvmStatic external fun scanQr(image: ByteArray, width: Int, height: Int, rowStride: Int,
                                   pixelStride: Int, network: Int): ByteArray?
    @JvmStatic external fun parsePayment(text: ByteArray, network: Int): ByteArray?
    @JvmStatic external fun createEntropy(): ByteArray?
    @JvmStatic external fun recoveryPhrase(entropy: ByteArray): CharArray?
    @JvmStatic external fun restoreEntropy(phrase: CharArray): ByteArray?
    @JvmStatic external fun confirmRecoveryPhrase(entropy: ByteArray, phrase: CharArray): Boolean
    @JvmStatic external fun receivingAddress(entropy: ByteArray, network: Int, index: Int): ByteArray?
    @JvmStatic external fun createWalletHeader(entropy: ByteArray, network: Int): ByteArray?
    @JvmStatic external fun recoveredWalletAddress(header: ByteArray, entropy: ByteArray): ByteArray?
    @JvmStatic external fun packWalletRecord(header: ByteArray, iv: ByteArray, ciphertext: ByteArray): ByteArray?
    @JvmStatic external fun unpackWalletRecord(record: ByteArray): Array<ByteArray>?
    @JvmStatic external fun readWalletStorage(directory: ByteArray): ByteArray?
    @JvmStatic external fun createWalletStorage(directory: ByteArray, record: ByteArray): Int
    @JvmStatic external fun promoteWalletStorage(directory: ByteArray, record: ByteArray): Int
    @JvmStatic external fun acceptWrappingPolicy(bits: Int, hardware: Int, flags: Int, seconds: Int, methods: Int): Boolean
}
