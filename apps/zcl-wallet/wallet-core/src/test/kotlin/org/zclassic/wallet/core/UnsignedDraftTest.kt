// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet.core

import java.util.concurrent.atomic.AtomicInteger
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertTrue

class UnsignedDraftTest {
    private fun fixture(name: String): ByteArray = checkNotNull(javaClass.getResourceAsStream("/review/$name"))
        .use { it.readBytes() }
    private fun funding() = listOf(
        UnsignedReview.Funding(fixture("previous0"), 0, 0xffff_ffffL),
        UnsignedReview.Funding(fixture("previous1"), 1, 0xffff_ffffL))
    private fun outputs(network: Network = Network.MAINNET) = listOf(
        UnsignedReview.Output(TransparentAddress.fromPublicKeyHash(ByteArray(20) { 0x55 }, network), Zatoshi.of(9000)),
        UnsignedReview.Output(TransparentAddress.fromScriptHash(ByteArray(20) { 0x66 }, network), Zatoshi.of(1500)))
    private fun prepare(funding: List<UnsignedReview.Funding> = funding(),
                        outputs: List<UnsignedReview.Output> = outputs(), network: Network = Network.MAINNET,
                        lockTime: Long = 0, expiryHeight: Long = 0, fee: Long = 500,
                        clock: () -> Long = { 100L }) =
        UnsignedReview.prepare(funding, outputs, network, lockTime, expiryHeight, Zatoshi.of(fee), clock)
    private fun failure(action: () -> Unit): CoreStatus = assertFailsWith<UnsignedReviewFailure>(block = action).status
    private fun native(parameters: LongArray): ByteArray = checkNotNull(NativeCore.buildDraft(
        arrayOf(fixture("previous0"), fixture("previous1")),
        outputs().map { it.address.encoded.toByteArray(Charsets.US_ASCII) }.toTypedArray(), parameters, 0))
    private fun parameters() = longArrayOf(0, 0, 500, 0, 0xffff_ffffL, 1, 0xffff_ffffL, 9000, 1500)

    @Test fun preparesExactUnsignedFixtureOnBothNetworksWithoutRetainingCallerSources() {
        for (network in Network.entries) {
            val funding = funding().toMutableList()
            val original = funding.map { it.previousTransaction.copyOf() }
            val outputs = outputs(network).toMutableList()
            prepare(funding, outputs, network).use { review ->
                funding.forEachIndexed { index, input ->
                    assertContentEquals(original[index], input.previousTransaction)
                    input.previousTransaction.fill(0)
                }
                funding.clear()
                outputs.clear()
                assertContentEquals(fixture("draft"), review.unsignedBytes())
                val snapshot = review.snapshot()
                assertEquals(network, snapshot.network)
                assertEquals("602c673db0503b48a400414347009ae968b1aaba9b10663bde508e7f8218dc46", snapshot.transactionId)
                assertEquals(500L, snapshot.fee.value)
                assertEquals(90000L, snapshot.remainingMillis)
                assertEquals(listOf("55".repeat(20), "66".repeat(20)), snapshot.outputs.map { it.hashHex })
            }
        }
    }

    @Test fun distinctIndexesCanShareSourceAndRawContextKeepsExplicitRowOrder() {
        val source = fixture("previous0")
        val funding = listOf(UnsignedReview.Funding(source, 1, 0x8000_0000L),
            UnsignedReview.Funding(source, 0, 0xffff_ffffL))
        val original = outputs()
        val outputs = listOf(original.last().copy(value = Zatoshi.of(5500)), original.first())
        prepare(funding, outputs, lockTime = 0xffff_ffffL, expiryHeight = 499_999_999).use { review ->
            val snapshot = review.snapshot()
            assertEquals(0xffff_ffffL, snapshot.lockTime)
            assertEquals(499_999_999L, snapshot.expiryHeight)
            assertEquals(listOf(1L, 0L), snapshot.inputs.map { it.previousIndex })
            assertEquals(listOf(0x8000_0000L, 0xffff_ffffL), snapshot.inputs.map { it.sequence })
            assertEquals(1, snapshot.inputs.map { it.previousTransactionId }.distinct().size)
            assertEquals(15000L, snapshot.inputTotal.value)
            assertEquals(14500L, snapshot.outputTotal.value)
            assertEquals(outputs.map { it.address }, snapshot.outputs.map { it.address() })
        }
    }

    @Test fun fieldFeeNetworkAndSourceFailuresDoNotLeaveAnActiveOwner() {
        assertEquals(CoreStatus.OUT_OF_RANGE, failure { prepare(fee = 499) })
        assertEquals(CoreStatus.OUT_OF_RANGE, failure { prepare(lockTime = -1) })
        assertEquals(CoreStatus.OUT_OF_RANGE, failure { prepare(expiryHeight = 500_000_000) })
        assertEquals(CoreStatus.UNSUPPORTED, failure { prepare(outputs = outputs(Network.TESTNET)) })
        val first = funding().first()
        assertEquals(CoreStatus.INVALID_ENCODING, failure { prepare(funding = listOf(first, first)) })
        assertEquals(CoreStatus.OUT_OF_RANGE, failure {
            prepare(funding = listOf(first.copy(sequence = -1), funding().last()))
        })
        val calls = AtomicInteger()
        assertFailsWith<UnsignedReviewFailure> {
            prepare(funding = listOf(first.copy(previousTransaction = byteArrayOf(0)), funding().last()),
                clock = { calls.incrementAndGet(); 100 })
        }
        assertEquals(0, calls.get())
        prepare().use { assertEquals(500L, it.snapshot().fee.value) }
    }

    @Test fun managedBoundsAndClockFailurePreserveCallerBytesAndReleaseTheSlot() {
        val first = funding().first()
        assertFailsWith<IllegalArgumentException> { prepare(funding = emptyList()) }
        assertFailsWith<IllegalArgumentException> { prepare(funding = List(9) { first }) }
        assertFailsWith<IllegalArgumentException> { prepare(outputs = emptyList()) }
        assertFailsWith<IllegalArgumentException> { prepare(outputs = List(17) { outputs().first() }) }
        val oversized = ByteArray(1926) { 0x5a }
        assertFailsWith<IllegalArgumentException> {
            prepare(funding = listOf(first.copy(previousTransaction = oversized)))
        }
        assertTrue(oversized.all { it == 0x5a.toByte() })
        val sources = funding()
        assertFailsWith<IllegalStateException> { prepare(funding = sources, clock = { error("Public clock failure") }) }
        assertContentEquals(fixture("previous0"), sources.first().previousTransaction)
        assertContentEquals(fixture("previous1"), sources.last().previousTransaction)
        prepare(funding = sources, clock = {
            // This occurs after construction, immediately before native review opening.
            sources.forEach { it.previousTransaction.fill(0) }
            100L
        }).use { assertContentEquals(fixture("draft"), it.unsignedBytes()) }
        prepare().close()
    }

    @Test fun preparingWhileBusyCannotCancelOrChangeTheExistingReview() {
        prepare().use { original ->
            assertEquals(CoreStatus.BUSY, failure { prepare() })
            assertContentEquals(fixture("draft"), original.unsignedBytes())
            val packet = native(parameters()) // Stateless construction itself owns no review slot.
            assertEquals(178, packet.size)
            assertEquals(0, packet[0].toInt())
            assertEquals(500L, original.snapshot().fee.value)
        }
        prepare().close()
    }

    @Test fun realJniRejectsWrongParameterShapesAndEverySignedWidthViolation() {
        val valid = parameters()
        for (length in 0..35) {
            if (length == valid.size) continue
            val packet = native(valid.copyOf(length))
            assertEquals(1, packet.size)
            assertTrue(packet[0].toInt() != CoreStatus.OK.code)
        }
        for (index in valid.indices) for (value in listOf(Long.MIN_VALUE, Long.MAX_VALUE)) {
            val changed = valid.copyOf().also { it[index] = value }
            val packet = native(changed)
            assertContentEquals(byteArrayOf(CoreStatus.OUT_OF_RANGE.code.toByte()), packet)
            assertEquals(value, changed[index])
        }
        assertContentEquals(fixture("draft"), native(valid).copyOfRange(1, 178))
        prepare().close()
    }
}
