// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet.core

import java.util.Collections

class UnsignedReviewFailure(val status: CoreStatus) : IllegalStateException("Unsigned review failed: $status")

/** Thin owner of one C unsigned draft. Prepare on a bounded worker, with a
 * trusted elapsed monotonic clock including sleep. Snapshot queries are bounded
 * metadata reads suitable for UI delivery, with no network I/O. C owns bytes,
 * accounting and expiry.
 * Close on background, lock or replacement; never persist/restore this owner.
 * Snapshots are public copies, not consent, key ownership or chain evidence.
 * A queued UI delivery must re-read its original foreground owner.
 * The native process permits one active review; another opening returns BUSY.
 */
class UnsignedReview private constructor(private var id: Long, clock: () -> Long) : AutoCloseable {
    private var clockSource: (() -> Long)? = clock

    /** Explicit public source selection. Bytes belong to the caller and must
     * remain stable during prepare; the factory retains only private copies
     * until C construction/opening completes. No funding or ownership proof. */
    data class Funding(val previousTransaction: ByteArray, val outputIndex: Long, val sequence: Long)
    /** Exact intended output; no implicit change destination or classification. */
    data class Output(val address: TransparentAddress, val value: Zatoshi)

    /** Exact public destination data; no change label or ownership assertion. */
    data class Destination(val network: Network, val kind: TransparentAddressKind,
                           val hashHex: String, val value: Zatoshi) {
        /** Canonical C encoding of this public destination, never a change label. */
        fun address(): TransparentAddress {
            require(hashHex.length == 40) { "Invalid review destination hash length" }
            val hash = hashHex.hexToByteArray()
            return when (kind) {
                TransparentAddressKind.P2PKH -> TransparentAddress.fromPublicKeyHash(hash, network)
                TransparentAddressKind.P2SH -> TransparentAddress.fromScriptHash(hash, network)
            }
        }
    }
    /** Lock/expiry/index/sequence are raw unsigned32 fields, carried exactly in Long. */
    data class Input(val previousTransactionId: String, val previousIndex: Long,
                     val sequence: Long, val destination: Destination)
    data class Snapshot(val network: Network, val transactionId: String, val lockTime: Long,
                        val expiryHeight: Long, val serializedSize: Int,
                        val inputTotal: Zatoshi, val outputTotal: Zatoshi, val fee: Zatoshi,
                        val maximumFee: Zatoshi, val inputs: List<Input>,
                        val outputs: List<Destination>, val remainingMillis: Long)

    fun snapshot(): Snapshot = read { handle, now ->
        decode(checkNotNull(NativeCore.reviewSnapshot(handle, now)) { "Native review snapshot failed" })
    }

    /** Independent public copy of the retained unsigned bytes, never a signature. */
    fun unsignedBytes(): ByteArray = read { handle, now ->
        decodeWire(checkNotNull(NativeCore.reviewWire(handle, now)) { "Native review bytes failed" })
    }

    @Synchronized private fun <T> read(operation: (Long, Long) -> T): T {
        if (id == 0L) throw UnsignedReviewFailure(CoreStatus.CANCELLED)
        try {
            val now = checkNotNull(clockSource) { "Closed review clock" }.invoke()
            return operation(id, now)
        } catch (problem: Throwable) {
            try { close() } catch (cleanup: Throwable) { problem.addSuppressed(cleanup) }
            throw problem
        }
    }

    @Synchronized override fun close() {
        val previous = id
        id = 0
        clockSource = null
        if (previous != 0L) cancelNative(previous)
    }

    companion object {
        /** Prepare on a bounded worker. Lists and source bytes must remain stable
         * for this synchronous call. C constructs and assesses the exact draft;
         * opening reuses those same privately copied sources. Temporary source
         * and draft byte copies clear on success/failure. Row order is explicit.
         * No coin selection, authenticated funding, change, signing or broadcast.
         */
        @Synchronized fun prepare(funding: List<Funding>, outputs: List<Output>, network: Network,
                                  lockTime: Long, expiryHeight: Long, maximumFee: Zatoshi,
                                  clock: () -> Long): UnsignedReview {
            val inputCount = funding.size
            val outputCount = outputs.size
            require(inputCount in 1..8 && outputCount in 1..16) { "Invalid draft row counts" }
            val sources = Array(inputCount) { ByteArray(0) }
            var draft: ByteArray? = null
            try {
                val parameters = LongArray(3 + 2 * inputCount + outputCount)
                parameters[0] = lockTime
                parameters[1] = expiryHeight
                parameters[2] = maximumFee.value
                for (index in sources.indices) {
                    val input = funding[index]
                    require(input.previousTransaction.size in 1..1925) { "Invalid previous transaction size" }
                    sources[index] = input.previousTransaction.copyOf()
                    parameters[3 + 2 * index] = input.outputIndex
                    parameters[4 + 2 * index] = input.sequence
                }
                val addresses = Array(outputCount) { index ->
                    val output = outputs[index]
                    parameters[3 + 2 * inputCount + index] = output.value.value
                    output.address.encoded.toByteArray(Charsets.US_ASCII)
                }
                val packet = checkNotNull(NativeCore.buildDraft(sources, addresses, parameters, network.nativeId)) {
                    "Native unsigned draft construction failed"
                }
                val prepared = decodeWire(packet)
                draft = prepared
                return open(prepared, sources, network, maximumFee, clock)
            } finally {
                draft?.fill(0)
                sources.forEach { it.fill(0) }
            }
        }

        /** Inputs must remain stable for this synchronous call. C copies all
         * bytes before publishing an ID; callers retain/dispose of their arrays.
         * Public synthetic previous bytes alone cannot establish spendability.
         */
        @Synchronized fun open(draft: ByteArray, previous: Array<ByteArray>, network: Network,
                               maximumFee: Zatoshi, clock: () -> Long): UnsignedReview {
            val result = NativeCore.openReview(draft, previous, network.nativeId, maximumFee.value, clock())
            return ownOpened(result, clock)
        }

        /** Explicit offline inspection of a bounded unsigned draft funded by
         * full v4 source bytes (including opaque shielded components). Inputs
         * remain stable for this call; C captures/copies at most eight sources
         * of 102000 bytes and retains only the checked review data. Proofs and
         * signatures in sources are not verified. No chain/unspentness, custody,
         * consent, signing or broadcast authority. Same foreground-only lifetime.
         */
        @Synchronized fun openFullSources(draft: ByteArray, previous: Array<ByteArray>, network: Network,
                                          maximumFee: Zatoshi, clock: () -> Long): UnsignedReview {
            val result = NativeCore.openFullSourceReview(draft, previous, network.nativeId, maximumFee.value, clock())
            return ownOpened(result, clock)
        }

        private fun ownOpened(result: Long, clock: () -> Long): UnsignedReview {
            if (result <= 0) {
                check(result in -Int.MAX_VALUE.toLong()..-1L) { "Invalid native review ID" }
                throw UnsignedReviewFailure(CoreStatus.fromCode((-result).toInt()))
            }
            try {
                return UnsignedReview(result, clock)
            } catch (problem: Throwable) {
                try { cancelNative(result) } catch (cleanup: Throwable) { problem.addSuppressed(cleanup) }
                throw problem
            }
        }

        private fun cancelNative(handle: Long) {
            val status = CoreStatus.fromCode(NativeCore.cancelReview(handle))
            if (status != CoreStatus.OK && status != CoreStatus.CANCELLED) throw UnsignedReviewFailure(status)
        }

        private fun decodeWire(packet: ByteArray): ByteArray {
            try {
                check(packet.size in 1..1926) { "Invalid native review byte packet" }
                val status = CoreStatus.fromCode(packet[0].toInt() and 0xff)
                if (status != CoreStatus.OK) {
                    check(packet.size == 1) { "Invalid native review failure packet" }
                    throw UnsignedReviewFailure(status)
                }
                check(packet.size > 1) { "Empty native review bytes" }
                return packet.copyOfRange(1, packet.size)
            } finally { packet.fill(0) }
        }

        private fun uint32(value: Long): Long {
            check(value in 0..0xffff_ffffL) { "Invalid native review uint32" }
            return value
        }

        private fun hashWords(values: LongArray, offset: Int, count: Int): String =
            buildString(count * 8) {
                repeat(count) { append(uint32(values[offset + it]).toString(16).padStart(8, '0')) }
            }

        private fun destination(values: LongArray, offset: Int, network: Network): Destination {
            val kind = when (values[offset + 1]) {
                1L -> TransparentAddressKind.P2PKH
                2L -> TransparentAddressKind.P2SH
                else -> error("Invalid native review destination kind")
            }
            return Destination(network, kind, hashWords(values, offset + 2, 5), Zatoshi.of(values[offset]))
        }

        internal fun decode(values: LongArray): Snapshot {
            check(values.size in 1..268) { "Invalid native review packet size" }
            check(values[0] in 0..Int.MAX_VALUE.toLong()) { "Invalid native review status" }
            val status = CoreStatus.fromCode(values[0].toInt())
            if (status != CoreStatus.OK) {
                check(values.size == 1) { "Invalid native review failure packet" }
                throw UnsignedReviewFailure(status)
            }
            check(values.size >= 20) { "Short native review header" }
            check(values[6] in 1L..8L && values[7] in 1L..16L) { "Invalid native review counts" }
            val count = values[6].toInt()
            val outputCount = values[7].toInt()
            check(values.size == 20 + 17 * count + 7 * outputCount) { "Invalid native review packet length" }
            check(values[1] in 1L..90_000L && values[5] in 1L..1925L) { "Invalid native review lifetime/size" }
            val network = when (values[2]) {
                0L -> Network.MAINNET
                1L -> Network.TESTNET
                else -> error("Invalid native review network")
            }
            val inputs = List(count) { index ->
                val offset = 20 + 17 * index
                Input(hashWords(values, offset, 8), uint32(values[offset + 8]),
                    uint32(values[offset + 9]), destination(values, offset + 10, network))
            }
            val outputs = List(outputCount) { index ->
                destination(values, 20 + 17 * count + 7 * index, network)
            }
            return Snapshot(network, hashWords(values, 12, 8), uint32(values[3]), uint32(values[4]),
                values[5].toInt(), Zatoshi.of(values[8]), Zatoshi.of(values[9]), Zatoshi.of(values[10]),
                Zatoshi.of(values[11]), Collections.unmodifiableList(inputs), Collections.unmodifiableList(outputs), values[1])
        }
    }
}
