// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith
import org.zclassic.wallet.core.CoreStatus
import org.zclassic.wallet.core.Network
import org.zclassic.wallet.core.UnsignedReview
import org.zclassic.wallet.core.UnsignedReviewFailure
import org.zclassic.wallet.core.Zatoshi
import java.security.MessageDigest

/** Public synthetic C-generated vectors only; no wallet, keys, chain or funds. */
@RunWith(AndroidJUnit4::class)
class FullSourceReviewInstrumentedTest {
    private fun fixture(name: String, path: String = "full-review"): ByteArray =
        InstrumentationRegistry.getInstrumentation().context.assets.open("$path/$name").use { it.readBytes() }
    private fun sources(): Array<ByteArray> = arrayOf(fixture("previous0"), fixture("previous1"))
    private fun open(clock: () -> Long = { 100L }): UnsignedReview =
        UnsignedReview.openFullSources(fixture("draft"), sources(), Network.MAINNET, Zatoshi.of(500), clock)
    private fun refused(status: CoreStatus, action: () -> Unit) {
        try { action() } catch (failure: UnsignedReviewFailure) { assertEquals(status, failure.status); return }
        throw AssertionError("Expected full-source review refusal")
    }
    private fun id(wire: ByteArray): String {
        val sha = MessageDigest.getInstance("SHA-256")
        return sha.digest(sha.digest(wire)).reversedArray().joinToString("") { "%02x".format(it.toInt() and 255) }
    }

    @Test fun fullSourcesRetireAfterExactOwnedReviewOnBothNetworks() {
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
                assertEquals(11000L, snapshot.inputTotal.value)
                assertEquals(10500L, snapshot.outputTotal.value)
                assertEquals(500L, snapshot.fee.value)
                assertEquals(listOf("55".repeat(20), "66".repeat(20)), snapshot.outputs.map { it.hashHex })
                assertArrayEquals(expected, review.unsignedBytes())
            }
        }
    }

    @Test fun changedAndOversizedSourcesRefuseWithoutKeepingAnOwner() {
        val previous = sources()
        previous[1][previous[1].lastIndex] = (previous[1].last().toInt() xor 1).toByte()
        refused(CoreStatus.INVALID_ENCODING) {
            UnsignedReview.openFullSources(fixture("draft"), previous, Network.MAINNET, Zatoshi.of(500)) { 100L }.close()
        }
        refused(CoreStatus.RESOURCE_EXHAUSTED) {
            UnsignedReview.openFullSources(fixture("draft"), arrayOf(ByteArray(102001), fixture("previous1")), Network.MAINNET, Zatoshi.of(500)) { 100L }.close()
        }
        refused(CoreStatus.OUT_OF_RANGE) {
            UnsignedReview.open(fixture("draft"), sources(), Network.MAINNET, Zatoshi.of(500)) { 100L }.close()
        }
        open().use { assertEquals(500L, it.snapshot().fee.value) }
    }

    @Test fun malformedDraftAndMismatchedSourceCountRefuseBeforeReviewOwnership() {
        val draft = fixture("draft")
        val before = draft.copyOf()
        val oversizedAggregate = Array(8) { ByteArray(102000) }
        refused(CoreStatus.INVALID_ARGUMENT) {
            UnsignedReview.openFullSources(draft, oversizedAggregate, Network.MAINNET, Zatoshi.of(500)) { 100L }.close()
        }
        assertArrayEquals(before, draft)
        oversizedAggregate.forEach { assertTrue(it.all { byte -> byte == 0.toByte() }) }
        val unsupported = draft.copyOf().also { it[0] = (it[0].toInt() xor 1).toByte() }
        for (invalid in arrayOf(ByteArray(0), draft.copyOf(draft.size - 1), unsupported)) {
            refused(if (invalid === unsupported) CoreStatus.UNSUPPORTED else CoreStatus.INVALID_ENCODING) {
                UnsignedReview.openFullSources(invalid, Array(2) { ByteArray(102000) }, Network.MAINNET, Zatoshi.of(500)) { 100L }.close()
            }
            open().use { assertArrayEquals(before, it.unsignedBytes()) }
        }
    }

    @Test fun expiryAndCrossProfileReplacementKeepTheSameLifetimeRules() {
        var now = 100L
        val old = open { now }
        old.use { review ->
            now = 90099
            assertEquals(1L, review.snapshot().remainingMillis)
            now = 90100
            refused(CoreStatus.TIMED_OUT) { review.snapshot() }
        }
        UnsignedReview.open(fixture("draft", "review"), arrayOf(fixture("previous0", "review"),
            fixture("previous1", "review")), Network.MAINNET, Zatoshi.of(500)) { 100L }.use { current ->
            old.close()
            refused(CoreStatus.BUSY) { open().close() }
            assertEquals(500L, current.snapshot().fee.value)
        }
        now = 100
        open { now }.use { review ->
            now = 99
            refused(CoreStatus.CANCELLED) { review.unsignedBytes() }
        }
        open().use { assertEquals(500L, it.snapshot().fee.value) }
    }
}
