// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith
import org.zclassic.wallet.core.*

/** Public synthetic fixtures only; no wallet, custody, funds or chain claim. */
@RunWith(AndroidJUnit4::class)
class FullSourcePreparationInstrumentedTest {
    private fun fixture(name: String): ByteArray =
        InstrumentationRegistry.getInstrumentation().context.assets.open("full-review/$name").use { it.readBytes() }
    private fun funding(): List<UnsignedReview.Funding> = List(2) {
        UnsignedReview.Funding(fixture("previous$it"), it.toLong(), 0xffff_ffffL)
    }
    private fun outputs(network: Network): List<UnsignedReview.Output> = listOf(
        UnsignedReview.Output(TransparentAddress.fromPublicKeyHash(ByteArray(20) { 0x55 }, network), Zatoshi.of(9000)),
        UnsignedReview.Output(TransparentAddress.fromScriptHash(ByteArray(20) { 0x66 }, network), Zatoshi.of(1500)))
    private fun prepare(inputs: List<UnsignedReview.Funding> = funding(), network: Network = Network.MAINNET,
                        fee: Long = 500, clock: () -> Long = { 100L }): UnsignedReview =
        UnsignedReview.prepareFullSources(inputs, outputs(network), network, 0, 0, Zatoshi.of(fee), clock)
    private fun refused(status: CoreStatus, action: () -> Unit) {
        try { action() } catch (failure: UnsignedReviewFailure) { assertEquals(status, failure.status); return }
        throw AssertionError("Expected full-source preparation refusal")
    }

    @Test fun exactOfflineConstructionOwnsItsBytesOnBothNetworks() {
        for (network in Network.entries) {
            val inputs = funding()
            val expected = inputs.map { it.previousTransaction.copyOf() }
            prepare(inputs, network).use { review ->
                inputs.forEachIndexed { index, input -> assertArrayEquals(expected[index], input.previousTransaction) }
                inputs.forEach { it.previousTransaction.fill(0) }
                val snapshot = review.snapshot()
                assertEquals(network, snapshot.network)
                assertEquals(listOf(0L, 1L), snapshot.inputs.map { it.previousIndex })
                assertEquals(11000L, snapshot.inputTotal.value)
                assertEquals(10500L, snapshot.outputTotal.value)
                assertEquals(500L, snapshot.fee.value)
                assertArrayEquals(fixture("draft"), review.unsignedBytes())
            }
        }
    }

    @Test fun refusalsAndCrossProfileContentionPreserveTheOwner() {
        refused(CoreStatus.OUT_OF_RANGE) { prepare(fee = 499).close() }
        refused(CoreStatus.RESOURCE_EXHAUSTED) {
            prepare(funding().map { it.copy(previousTransaction = ByteArray(102001)) }).close()
        }
        refused(CoreStatus.INVALID_ENCODING) {
            prepare(funding().map { it.copy(previousTransaction = it.previousTransaction.copyOf(it.previousTransaction.size - 1)) }).close()
        }
        prepare().use { review ->
            refused(CoreStatus.BUSY) { prepare().close() }
            refused(CoreStatus.BUSY) {
                UnsignedReview.openFullSources(fixture("draft"), funding().map { it.previousTransaction }.toTypedArray(),
                    Network.MAINNET, Zatoshi.of(500)) { 100L }.close()
            }
            assertArrayEquals(fixture("draft"), review.unsignedBytes())
        }
    }

    @Test fun expiryRollbackAndReplacementRetainNativeLifetimeRules() {
        var now = 100L
        val old = prepare(clock = { now })
        old.use { review ->
            now = 90099
            assertEquals(1L, review.snapshot().remainingMillis)
            now = 90100
            refused(CoreStatus.TIMED_OUT) { review.snapshot() }
        }
        now = 100
        prepare(clock = { now }).use { review ->
            old.close()
            assertEquals(500L, review.snapshot().fee.value)
            now = 99
            refused(CoreStatus.CANCELLED) { review.unsignedBytes() }
        }
        prepare().use { assertArrayEquals(fixture("draft"), it.unsignedBytes()) }
    }
}
