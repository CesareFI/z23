// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet.core

import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith

/** Only public synthetic source bytes with invalid opaque proofs/signatures. */
class FullSourcePreparationTest {
    private fun fixture(name: String): ByteArray =
        checkNotNull(javaClass.getResourceAsStream("/full-review/$name")).use { it.readBytes() }
    private fun funding(): List<UnsignedReview.Funding> = List(2) {
        UnsignedReview.Funding(fixture("previous$it"), it.toLong(), 0xffff_ffffL)
    }
    private fun outputs(network: Network = Network.MAINNET): List<UnsignedReview.Output> = listOf(
        UnsignedReview.Output(TransparentAddress.fromPublicKeyHash(ByteArray(20) { 0x55 }, network), Zatoshi.of(9000)),
        UnsignedReview.Output(TransparentAddress.fromScriptHash(ByteArray(20) { 0x66 }, network), Zatoshi.of(1500)))
    private fun prepare(inputs: List<UnsignedReview.Funding> = funding(),
                        destinations: List<UnsignedReview.Output> = outputs(), network: Network = Network.MAINNET,
                        fee: Long = 500, clock: () -> Long = { 100L }): UnsignedReview =
        UnsignedReview.prepareFullSources(inputs, destinations, network, 0, 0, Zatoshi.of(fee), clock)
    private fun refused(status: CoreStatus, action: () -> Unit) {
        assertEquals(status, assertFailsWith<UnsignedReviewFailure>(block = action).status)
    }

    @Test fun oneCallConstructsExactOwnedWireOnBothNetworks() {
        for (network in Network.entries) {
            val inputs = funding()
            val original = inputs.map { it.previousTransaction.copyOf() }
            prepare(inputs, outputs(network), network).use { review ->
                inputs.forEachIndexed { index, input -> assertContentEquals(original[index], input.previousTransaction) }
                inputs.forEach { it.previousTransaction.fill(0) }
                val snapshot = review.snapshot()
                assertEquals(network, snapshot.network)
                assertEquals(listOf(0L, 1L), snapshot.inputs.map { it.previousIndex })
                assertEquals(listOf(0xffff_ffffL, 0xffff_ffffL), snapshot.inputs.map { it.sequence })
                assertEquals(11000L, snapshot.inputTotal.value)
                assertEquals(10500L, snapshot.outputTotal.value)
                assertEquals(500L, snapshot.fee.value)
                assertContentEquals(fixture("draft"), review.unsignedBytes())
            }
        }
    }

    @Test fun exactSelectionsAndOutputOrderRemainExplicit() {
        val inputs = funding().reversed().map { it.copy(sequence = 0x8000_0000L) }
        val destinations = outputs().reversed()
        UnsignedReview.prepareFullSources(inputs, destinations, Network.MAINNET,
            0xffff_ffffL, 499999999L, Zatoshi.of(500)) { 100L }.use { review ->
            val snapshot = review.snapshot()
            assertEquals(0xffff_ffffL, snapshot.lockTime)
            assertEquals(499999999L, snapshot.expiryHeight)
            assertEquals(listOf(1L, 0L), snapshot.inputs.map { it.previousIndex })
            assertEquals(listOf(0x8000_0000L, 0x8000_0000L), snapshot.inputs.map { it.sequence })
            assertEquals(destinations.map { it.address }, snapshot.outputs.map { it.address() })
        }
    }

    @Test fun refusedSelectionsNeverLeaveAnOwner() {
        refused(CoreStatus.OUT_OF_RANGE) { prepare(fee = 499).close() }
        refused(CoreStatus.UNSUPPORTED) { prepare(destinations = outputs(Network.TESTNET)).close() }
        refused(CoreStatus.OUT_OF_RANGE) { prepare(funding().map { it.copy(outputIndex = -1) }).close() }
        refused(CoreStatus.OUT_OF_RANGE) { prepare(funding().map { it.copy(sequence = 0x1_0000_0000L) }).close() }
        refused(CoreStatus.INVALID_ENCODING) {
            prepare(funding().map { it.copy(previousTransaction = it.previousTransaction.copyOf(it.previousTransaction.size - 1)) }).close()
        }
        refused(CoreStatus.RESOURCE_EXHAUSTED) {
            prepare(funding().map { it.copy(previousTransaction = ByteArray(102001)) }).close()
        }
        assertFailsWith<IllegalArgumentException> { prepare(emptyList()).close() }
        prepare().use { assertContentEquals(fixture("draft"), it.unsignedBytes()) }
    }

    @Test fun prepareAndBothOpenProfilesShareTheSameOwner() {
        val inputs = funding()
        val old = prepare(inputs)
        old.use {
            refused(CoreStatus.BUSY) { prepare().close() }
            refused(CoreStatus.BUSY) {
                UnsignedReview.openFullSources(fixture("draft"), inputs.map { it.previousTransaction }.toTypedArray(),
                    Network.MAINNET, Zatoshi.of(500)) { 100L }.close()
            }
            assertFailsWith<IllegalArgumentException> {
                UnsignedReview.prepare(inputs, outputs(), Network.MAINNET, 0, 0, Zatoshi.of(500)) { 100L }.close()
            }
        }
        prepare().use { current ->
            old.close()
            assertEquals(500L, current.snapshot().fee.value)
        }
    }

    @Test fun expiryRollbackAndClockExceptionsRetirePreparation() {
        var now = 100L
        prepare(clock = { now }).use { review ->
            now = 90099
            assertEquals(1L, review.snapshot().remainingMillis)
            now = 90100
            refused(CoreStatus.TIMED_OUT) { review.unsignedBytes() }
        }
        now = 100
        prepare(clock = { now }).use { review ->
            now = 99
            refused(CoreStatus.CANCELLED) { review.snapshot() }
        }
        assertFailsWith<IllegalStateException> { prepare(clock = { error("fixture clock") }).close() }
        var fail = false
        prepare(clock = { if (fail) error("fixture clock") else 100L }).use { review ->
            fail = true
            assertFailsWith<IllegalStateException> { review.snapshot() }
            prepare().use { assertEquals(500L, it.snapshot().fee.value) }
        }
    }
}
