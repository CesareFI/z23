// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet.core

import java.security.MessageDigest
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertTrue

/** Public synthetic opaque proofs/signatures; no chain or spendability claim. */
class FullSourceReviewTest {
    private fun fixture(name: String, path: String = "full-review"): ByteArray =
        checkNotNull(javaClass.getResourceAsStream("/$path/$name")).use { it.readBytes() }
    private fun sources(): Array<ByteArray> = arrayOf(fixture("previous0"), fixture("previous1"))
    private fun open(clock: () -> Long = { 100L }): UnsignedReview =
        UnsignedReview.openFullSources(fixture("draft"), sources(), Network.MAINNET, Zatoshi.of(500), clock)
    private fun legacy(): UnsignedReview = UnsignedReview.open(fixture("draft", "review"),
        arrayOf(fixture("previous0", "review"), fixture("previous1", "review")), Network.MAINNET, Zatoshi.of(500)) { 100L }
    private fun id(wire: ByteArray): String {
        val sha = MessageDigest.getInstance("SHA-256")
        return sha.digest(sha.digest(wire)).reversedArray().joinToString("") { "%02x".format(it.toInt() and 255) }
    }
    private fun refused(status: CoreStatus, action: () -> Unit) {
        assertEquals(status, assertFailsWith<UnsignedReviewFailure>(block = action).status)
    }

    @Test fun realVmOwnsExactFullSourcesOnBothNetworks() {
        for (network in Network.entries) {
            val draft = fixture("draft")
            val expected = draft.copyOf()
            val previous = sources()
            val identities = previous.map(::id)
            UnsignedReview.openFullSources(draft, previous, network, Zatoshi.of(500)) { 100L }.use { review ->
                draft.fill(0); previous.forEach { it.fill(0) }
                val snapshot = review.snapshot()
                assertEquals(network, snapshot.network)
                assertEquals(id(expected), snapshot.transactionId)
                assertEquals(identities, snapshot.inputs.map { it.previousTransactionId })
                assertEquals(listOf(0L, 1L), snapshot.inputs.map { it.previousIndex })
                assertEquals(11000L, snapshot.inputTotal.value)
                assertEquals(10500L, snapshot.outputTotal.value)
                assertEquals(500L, snapshot.fee.value)
                assertEquals(listOf("55".repeat(20), "66".repeat(20)), snapshot.outputs.map { it.hashHex })
                assertContentEquals(expected, review.unsignedBytes())
                review.unsignedBytes().fill(0)
                assertContentEquals(expected, review.unsignedBytes())
            }
        }
    }

    @Test fun profilesShareOneOwnerAndLegacyAdmissionStaysNarrow() {
        refused(CoreStatus.OUT_OF_RANGE) {
            UnsignedReview.open(fixture("draft"), sources(), Network.MAINNET, Zatoshi.of(500)) { 100L }.close()
        }
        val old = open()
        old.use { refused(CoreStatus.BUSY) { legacy().close() } }
        legacy().use { refused(CoreStatus.BUSY) { open().close() } }
        open().use { current ->
            old.close()
            refused(CoreStatus.CANCELLED) { old.snapshot() }
            assertEquals(500L, current.snapshot().fee.value)
        }
    }

    @Test fun allSourceRefusalsLeaveTheNativeOwnerAvailable() {
        val draft = fixture("draft")
        val invalid = listOf(
            emptyArray<ByteArray>() to CoreStatus.OUT_OF_RANGE,
            Array(9) { ByteArray(1) } to CoreStatus.OUT_OF_RANGE,
            arrayOf(ByteArray(0), fixture("previous1")) to CoreStatus.OUT_OF_RANGE,
            arrayOf(ByteArray(102001), fixture("previous1")) to CoreStatus.RESOURCE_EXHAUSTED,
            arrayOf(fixture("previous0").dropLast(1).toByteArray(), fixture("previous1")) to CoreStatus.INVALID_ENCODING,
            sources().also { it[1][it[1].lastIndex] = (it[1].last().toInt() xor 1).toByte() } to CoreStatus.INVALID_ENCODING)
        for ((previous, status) in invalid) {
            refused(status) { UnsignedReview.openFullSources(draft, previous, Network.MAINNET, Zatoshi.of(500)) { 100L }.close() }
            open().use { assertEquals(500L, it.snapshot().fee.value) }
        }
    }

    @Test fun expiryRollbackAndClockFailureCancelOnlyTheirOwner() {
        var now = 100L
        open { now }.use { review ->
            now = 90099
            assertEquals(1L, review.snapshot().remainingMillis)
            now = 90100
            refused(CoreStatus.TIMED_OUT) { review.unsignedBytes() }
        }
        now = 100
        open { now }.use { review ->
            now = 99
            refused(CoreStatus.CANCELLED) { review.snapshot() }
        }
        var fail = false
        open { if (fail) error("fixture clock refusal") else 100L }.use { review ->
            fail = true
            assertFailsWith<IllegalStateException> { review.snapshot() }
            open().use { assertTrue(it.snapshot().remainingMillis > 0) }
        }
    }
}
