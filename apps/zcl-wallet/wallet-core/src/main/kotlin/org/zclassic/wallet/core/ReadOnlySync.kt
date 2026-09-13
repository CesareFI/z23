// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet.core

class ReadOnlySyncFailure(val status: CoreStatus) : IllegalStateException("Read-only sync failed: $status")

/** Thin synchronous JNI owner. C owns protocol, bounds, clocks and balance
 * policy. This class has no network, persistence, key or signing capability.
 * Use on a bounded worker and close on foreground/source/address replacement.
 * Supply one elapsed monotonic clock including sleep. It is sampled inside the
 * owner's lock so concurrent callers cannot reorder pre-sampled timestamps.
 * Never save/restore snapshots as current state. The four-slot native registry
 * is shared by these owners.
 */
class ReadOnlySync(val address: TransparentAddress, sourceIdentity: ByteArray,
                   clock: () -> Long) : AutoCloseable {
    private var clockSource: (() -> Long)? = clock
    private val source: ByteArray = sourceIdentity.also {
        require(it.size == 32) { "Invalid sync source identity size" }
    }.copyOf()
    private var owner = positive(NativeCore.openSyncOwner(
        address.encoded.toByteArray(Charsets.US_ASCII), address.network.nativeId, source))

    enum class Freshness { UNAVAILABLE, UNVERIFIED, STALE }
    data class Report(val confirmed: Zatoshi, val pendingDelta: Long, val total: Zatoshi, val height: Long)
    /** nextChangeDelayMillis is C's relative wakeup hint; zero means none.
     * Read again at delivery time. Never turn the hint into a freshness decision.
     */
    data class Snapshot(val freshness: Freshness, val refreshing: Boolean,
                        val lastFault: CoreStatus, val ageMillis: Long, val report: Report?,
                        val nextChangeDelayMillis: Long)

    /** The owner reference is immutable. Never dispatch this token on a new owner. */
    class Attempt internal constructor(private val owner: ReadOnlySync, private val token: Long) {
        fun request(): ByteArray = owner.request(token)
        /** frame must remain stable during this synchronous call. */
        fun reply(frame: ByteArray): CoreStatus = owner.reply(token, frame)
        fun fail(reason: CoreStatus = CoreStatus.CANCELLED): CoreStatus = owner.fail(token, reason)
    }

    fun sourceIdentity(): ByteArray = source.copyOf()

    @Synchronized fun begin(timeoutMillis: Long = 30_000, firstId: Long = 1): Attempt {
        requireOpen()
        val token = positive(NativeCore.beginSyncAttempt(owner, now(), timeoutMillis, firstId))
        try {
            return Attempt(this, token)
        } catch (problem: Throwable) {
            NativeCore.failSyncAttempt(owner, token, CoreStatus.RESOURCE_EXHAUSTED.code)
            throw problem
        }
    }

    @Synchronized private fun request(token: Long): ByteArray {
        requireOpen()
        val packet = checkNotNull(NativeCore.syncRequest(owner, token, now())) { "Native sync request failed" }
        check(packet.isNotEmpty()) { "Empty native sync request" }
        requireSuccess(packet[0].toInt() and 0xff)
        try {
            return packet.copyOfRange(1, packet.size)
        } catch (problem: Throwable) {
            NativeCore.failSyncAttempt(owner, token, CoreStatus.RESOURCE_EXHAUSTED.code)
            throw problem
        }
    }

    @Synchronized private fun reply(token: Long, frame: ByteArray): CoreStatus {
        if (owner == 0L) return CoreStatus.CANCELLED
        return CoreStatus.fromCode(NativeCore.syncReply(owner, token, now(), frame))
    }

    @Synchronized private fun fail(token: Long, reason: CoreStatus): CoreStatus {
        if (owner == 0L) return CoreStatus.CANCELLED
        return CoreStatus.fromCode(NativeCore.failSyncAttempt(owner, token, reason.code))
    }

    @Synchronized fun snapshot(): Snapshot {
        requireOpen()
        val packet = checkNotNull(NativeCore.syncSnapshot(owner, now())) { "Native sync snapshot failed" }
        check(packet.size == 10) { "Invalid native sync snapshot size" }
        requireSuccess(packet[0].toInt())
        check(packet[1] in 0L..2L && packet[2] in 0L..1L) { "Invalid native sync snapshot flags" }
        check(packet[9] >= 0) { "Invalid native sync wakeup delay" }
        val freshness = Freshness.entries[packet[1].toInt()]
        val report = if (freshness == Freshness.UNAVAILABLE) null else
            Report(Zatoshi.of(packet[5]), packet[6], Zatoshi.of(packet[7]), packet[8])
        return Snapshot(freshness, packet[2] == 1L, CoreStatus.fromCode(packet[3].toInt()),
                        packet[4], report, packet[9])
    }

    @Synchronized override fun close() {
        val previous = owner
        owner = 0
        clockSource = null
        if (previous != 0L) requireSuccess(NativeCore.closeSyncOwner(previous))
    }

    private fun requireOpen() {
        if (owner == 0L) throw ReadOnlySyncFailure(CoreStatus.CANCELLED)
    }

    private fun now(): Long = checkNotNull(clockSource) { "Sync clock is unavailable" }.invoke()

    companion object {
        private fun requireSuccess(code: Int) {
            val status = CoreStatus.fromCode(code)
            if (status != CoreStatus.OK) throw ReadOnlySyncFailure(status)
        }

        private fun positive(value: Long): Long {
            if (value > 0) return value
            check(value in -CoreStatus.TLS_FAILURE.code.toLong()..-1L) { "Invalid native sync identifier" }
            throw ReadOnlySyncFailure(CoreStatus.fromCode((-value).toInt()))
        }
    }
}
