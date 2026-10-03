// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet.core

import java.security.MessageDigest
import java.util.concurrent.CountDownLatch
import java.util.concurrent.Executors
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicInteger
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import kotlin.test.assertFails
import kotlin.test.assertFailsWith
import kotlin.test.assertTrue

class UnsignedReviewTest {
    private fun fixture(name: String): ByteArray = checkNotNull(javaClass.getResourceAsStream("/review/$name"))
        .use { it.readBytes() }
    private fun previous(): Array<ByteArray> = arrayOf(fixture("previous0"), fixture("previous1"))
    private fun open(clock: () -> Long = { 100L }): UnsignedReview =
        UnsignedReview.open(fixture("draft"), previous(), Network.MAINNET, Zatoshi.of(500), clock)
    private fun id(bytes: ByteArray): String {
        val sha = MessageDigest.getInstance("SHA-256")
        return sha.digest(sha.digest(bytes)).reversedArray().joinToString("") { "%02x".format(it.toInt() and 255) }
    }

    @Test fun nativeOpeningRefusesOutOfRangeFeeAndAcceptsExactMaximum() {
        val draft = fixture("draft")
        val sources = previous()
        for (entry in listOf(NativeCore::openReview, NativeCore::openFullSourceReview)) {
            for (fee in listOf(-1L, Zatoshi.MAX_VALUE + 1, Long.MAX_VALUE)) {
                val result = entry(draft, sources, 0, fee, 100)
                if (result > 0) NativeCore.cancelReview(result)
                assertEquals(-CoreStatus.OUT_OF_RANGE.code.toLong(), result)
            }
            val handle = entry(draft, sources, 0, Zatoshi.MAX_VALUE, 100)
            assertTrue(handle > 0)
            try {
                val snapshot = UnsignedReview.decode(checkNotNull(NativeCore.reviewSnapshot(handle, 100)))
                assertEquals(Zatoshi.MAX_VALUE, snapshot.maximumFee.value)
            } finally {
                assertEquals(CoreStatus.OK.code, NativeCore.cancelReview(handle))
            }
        }
        assertContentEquals(fixture("draft"), draft)
        previous().zip(sources).forEach { (expected, actual) -> assertContentEquals(expected, actual) }
    }

    @Test fun copiesExactPublicContextAndAccounting() {
        val draft = fixture("draft")
        val sources = previous()
        var now = 100L
        UnsignedReview.open(draft, sources, Network.TESTNET, Zatoshi.of(500)) { now }.use { review ->
            val expected = draft.copyOf()
            val expectedPrevious = sources.map(::id)
            draft.fill(0)
            sources.forEach { it.fill(0) }
            now = 101
            val snapshot = review.snapshot()
            assertEquals(Network.TESTNET, snapshot.network)
            assertEquals("602c673db0503b48a400414347009ae968b1aaba9b10663bde508e7f8218dc46", snapshot.transactionId)
            assertEquals(id(expected), snapshot.transactionId)
            assertEquals(0L, snapshot.lockTime)
            assertEquals(0L, snapshot.expiryHeight)
            assertEquals(expected.size, snapshot.serializedSize)
            assertEquals(89999L, snapshot.remainingMillis)
            assertEquals(11000L, snapshot.inputTotal.value)
            assertEquals(10500L, snapshot.outputTotal.value)
            assertEquals(500L, snapshot.fee.value)
            assertEquals(500L, snapshot.maximumFee.value)
            assertEquals(expectedPrevious, snapshot.inputs.map { it.previousTransactionId })
            assertEquals(listOf(0L, 1L), snapshot.inputs.map { it.previousIndex })
            assertTrue(snapshot.inputs.all { it.sequence == 0xffff_ffffL })
            assertEquals(listOf("11".repeat(20), "44".repeat(20)), snapshot.inputs.map { it.destination.hashHex })
            assertEquals(listOf("55".repeat(20), "66".repeat(20)), snapshot.outputs.map { it.hashHex })
            assertEquals(listOf(TransparentAddressKind.P2PKH, TransparentAddressKind.P2SH), snapshot.outputs.map { it.kind })
            assertTrue(snapshot.outputs.all { it.network == Network.TESTNET })
            assertContentEquals(("76a914" + "55".repeat(20) + "88ac").hexToByteArray(),
                snapshot.outputs.first().address().scriptPubKey())
            assertContentEquals(("a914" + "66".repeat(20) + "87").hexToByteArray(),
                snapshot.outputs.last().address().scriptPubKey())
            assertTrue(snapshot.outputs.all { it.address().network == Network.TESTNET })
            assertFailsWith<UnsupportedOperationException> { (snapshot.inputs as MutableList<*>).clear() }
            assertFailsWith<UnsupportedOperationException> { (snapshot.outputs as MutableList<*>).clear() }
            val bytes = review.unsignedBytes()
            assertContentEquals(expected, bytes)
            bytes.fill(0)
            assertContentEquals(expected, review.unsignedBytes())
        }
    }

    @Test fun busyCloseAndOldOwnersCannotReachReplacement() {
        var oldClockCalls = 0
        val first = open { oldClockCalls++; 100 }
        try {
            assertEquals(CoreStatus.BUSY, assertFailsWith<UnsignedReviewFailure> { open() }.status)
            assertEquals(500L, first.snapshot().fee.value)
        } finally { first.close() }
        open().use { replacement ->
            val before = oldClockCalls
            assertEquals(CoreStatus.CANCELLED, assertFailsWith<UnsignedReviewFailure> { first.snapshot() }.status)
            first.close()
            assertEquals(before, oldClockCalls)
            assertEquals(500L, replacement.snapshot().fee.value)
        }
    }

    @Test fun deadlineRollbackAndClockFailureReleaseOwner() {
        var now = 100L
        open { now }.use { review ->
            now = 90099
            assertEquals(1L, review.snapshot().remainingMillis)
            now = 90100
            assertEquals(CoreStatus.TIMED_OUT, assertFailsWith<UnsignedReviewFailure> { review.snapshot() }.status)
        }
        now = 100
        open { now }.use { review ->
            now = 99
            assertEquals(CoreStatus.CANCELLED, assertFailsWith<UnsignedReviewFailure> { review.unsignedBytes() }.status)
        }
        var fail = false
        val failedClock = open { if (fail) error("Fixture clock failure") else 100 }
        fail = true
        assertFailsWith<IllegalStateException> { failedClock.snapshot() }
        open().use { assertEquals(500L, it.snapshot().fee.value) }
        failedClock.close()
    }

    @Test fun invalidInputsAndNegativeClockDoNotLeaveBusyDraft() {
        assertEquals(CoreStatus.OUT_OF_RANGE, assertFailsWith<UnsignedReviewFailure> {
            UnsignedReview.open(fixture("draft"), previous(), Network.MAINNET, Zatoshi.of(499)) { 100 }
        }.status)
        assertFailsWith<UnsignedReviewFailure> {
            UnsignedReview.open(byteArrayOf(0), previous(), Network.MAINNET, Zatoshi.of(500)) { 100 }
        }
        var now = 100L
        open { now }.use { review ->
            now = -1
            assertEquals(CoreStatus.OUT_OF_RANGE, assertFailsWith<UnsignedReviewFailure> { review.snapshot() }.status)
            open().use { assertEquals(500L, it.snapshot().fee.value) }
        }
    }

    @Test fun samplesClockInsideReadSerialization() {
        val calls = AtomicInteger()
        val entered = CountDownLatch(1)
        val release = CountDownLatch(1)
        val secondStarted = CountDownLatch(1)
        val executor = Executors.newFixedThreadPool(2)
        val review = open {
            val call = calls.incrementAndGet()
            if (call == 2) { entered.countDown(); check(release.await(3, TimeUnit.SECONDS)) }
            100L + call
        }
        try {
            val first = executor.submit<UnsignedReview.Snapshot> { review.snapshot() }
            assertTrue(entered.await(3, TimeUnit.SECONDS))
            val second = executor.submit<UnsignedReview.Snapshot> { secondStarted.countDown(); review.snapshot() }
            assertTrue(secondStarted.await(3, TimeUnit.SECONDS))
            assertFailsWith<java.util.concurrent.TimeoutException> { second.get(100, TimeUnit.MILLISECONDS) }
            assertEquals(2, calls.get())
            release.countDown()
            assertEquals(89999L, first.get(3, TimeUnit.SECONDS).remainingMillis)
            assertEquals(89998L, second.get(3, TimeUnit.SECONDS).remainingMillis)
        } finally {
            release.countDown()
            review.close()
            executor.shutdownNow()
            assertTrue(executor.awaitTermination(3, TimeUnit.SECONDS))
        }
    }

    @Test fun rejectsMalformedNativePacketShapesAndWidths() {
        val handle = NativeCore.openReview(fixture("draft"), previous(), 0, 500, 100)
        assertTrue(handle > 0)
        val original = try { checkNotNull(NativeCore.reviewSnapshot(handle, 100)) }
            finally { assertEquals(CoreStatus.OK.code, NativeCore.cancelReview(handle)) }
        assertEquals(68, original.size)
        for (length in 0 until original.size) assertFails { UnsignedReview.decode(original.copyOf(length)) }
        assertFails { UnsignedReview.decode(original.copyOf(269)) }
        val mutations = listOf(0 to Long.MIN_VALUE, 1 to 0L, 1 to 90001L, 2 to 2L,
            3 to -1L, 4 to 0x1_0000_0000L, 5 to 1926L, 6 to 9L, 7 to 17L,
            8 to -1L, 9 to Long.MAX_VALUE, 10 to -1L, 11 to Long.MAX_VALUE,
            12 to -1L, 20 to 0x1_0000_0000L, 28 to -1L, 29 to Long.MAX_VALUE,
            31 to 3L, 32 to -1L, 55 to 0L)
        for ((index, value) in mutations) {
            val changed = original.copyOf().also { it[index] = value }
            assertFails("packet field $index") { UnsignedReview.decode(changed) }
        }
        original.fill(0)
        open().use { assertEquals(500L, it.snapshot().fee.value) }
    }
}
