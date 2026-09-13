// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet.core

/** Stable C status values. No input, path, ciphertext or secrets in errors. */
enum class CoreStatus(internal val code: Int) {
    OK(0), INVALID_ARGUMENT(1), INVALID_ENCODING(2), OUT_OF_RANGE(3), BUFFER_TOO_SMALL(4),
    UNSUPPORTED(5), CRYPTO_FAILURE(6), IO_FAILURE(7), INVALID_CHILD(8), RESOURCE_EXHAUSTED(9),
    NOT_FOUND(10), ALREADY_EXISTS(11), BUSY(12), IO_UNCERTAIN(13),
    TIMED_OUT(14), CANCELLED(15), TLS_FAILURE(16);

    companion object {
        internal fun fromCode(code: Int): CoreStatus = entries.single { it.code == code }
    }
}

/** Thin path/JNI adapter. Directory must be supplied by Android private
 * storage, never user/network input. The C owner handles files, locking,
 * validation and crash recovery. There is deliberately no erase/overwrite API.
 */
class WalletStorage(directory: String) {
    private val path: ByteArray

    init {
        require(directory.length <= 1024) { "Private storage path is too long" }
        path = directory.toByteArray(Charsets.UTF_8)
    }

    class ReadResult internal constructor(
        val status: CoreStatus,
        val record: ByteArray? = null,
        val pending: Boolean = false,
    )

    fun read(): ReadResult {
        val packet = checkNotNull(NativeCore.readWalletStorage(path)) { "Native storage read failed" }
        check(packet.isNotEmpty()) { "Empty native storage result" }
        val status = CoreStatus.fromCode(packet[0].toInt() and 0xff)
        if (status != CoreStatus.OK) return ReadResult(status)
        check(packet.size >= 2 && packet[1].toInt() in 0..1) { "Invalid native storage result" }
        return ReadResult(status, packet.copyOfRange(2, packet.size), packet[1].toInt() == 1)
    }

    /** Wallet-only persistence, including restoration with unknown change
     * history. This never initializes or repairs a change counter. */
    fun create(encryptedRecord: ByteArray): CoreStatus =
        CoreStatus.fromCode(NativeCore.createWalletStorage(path, encryptedRecord))

    /** Fresh generated/confirmed entropy only, after per-use platform GCM has
     * encrypted this exact record. Never use for a restored seed or migration.
     * C durably creates authenticated state0 before committing the wallet.
     * Inputs remain stable during this worker-thread call; caller clears entropy
     * afterward. Failure preserves all artifacts and never resets existing state. */
    fun createFreshWithChange(encryptedRecord: ByteArray, freshEntropy: ByteArray): CoreStatus =
        CoreStatus.fromCode(NativeCore.createFreshWalletStorage(path, encryptedRecord, freshEntropy))

    /** Caller must authenticate GCM and verify the recovered address first. */
    fun promote(authenticatedRecord: ByteArray): CoreStatus =
        CoreStatus.fromCode(NativeCore.promoteWalletStorage(path, authenticatedRecord))
}
