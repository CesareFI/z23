// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.os.SystemClock
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith
import org.zclassic.wallet.core.CoreStatus
import org.zclassic.wallet.core.Network
import org.zclassic.wallet.core.TransparentAddressKind
import org.zclassic.wallet.core.UnsignedReview
import org.zclassic.wallet.core.UnsignedReviewFailure
import org.zclassic.wallet.core.Zatoshi

/** Public synthetic transactions only. No wallet, authentication, network or funds. */
@RunWith(AndroidJUnit4::class)
class UnsignedReviewInstrumentedTest {
    private fun fixture(name: String): ByteArray = InstrumentationRegistry.getInstrumentation()
        .context.assets.open("review/$name").use { it.readBytes() }
    private fun previous(): Array<ByteArray> = arrayOf(fixture("previous0"), fixture("previous1"))
    private fun open(clock: () -> Long = SystemClock::elapsedRealtime): UnsignedReview =
        UnsignedReview.open(fixture("draft"), previous(), Network.MAINNET, Zatoshi.of(500), clock)
    private fun failure(action: () -> Unit): CoreStatus {
        try { action() } catch (problem: UnsignedReviewFailure) { return problem.status }
        throw AssertionError("Expected review refusal")
    }

    @Test fun realVmCopiesPublicUnsignedDraftOnBothNetworks() {
        for (network in Network.entries) {
            val draft = fixture("draft")
            val expected = draft.copyOf()
            val sources = previous()
            UnsignedReview.open(draft, sources, network, Zatoshi.of(500), SystemClock::elapsedRealtime).use { review ->
                draft.fill(0)
                sources.forEach { it.fill(0) }
                val snapshot = review.snapshot()
                assertEquals(network, snapshot.network)
                assertEquals("602c673db0503b48a400414347009ae968b1aaba9b10663bde508e7f8218dc46", snapshot.transactionId)
                assertEquals(11000L, snapshot.inputTotal.value)
                assertEquals(10500L, snapshot.outputTotal.value)
                assertEquals(500L, snapshot.fee.value)
                assertEquals(500L, snapshot.maximumFee.value)
                assertEquals(0L, snapshot.lockTime)
                assertEquals(0L, snapshot.expiryHeight)
                assertEquals(2, snapshot.inputs.size)
                assertEquals(0xffff_ffffL, snapshot.inputs.last().sequence)
                assertEquals(1L, snapshot.inputs.last().previousIndex)
                assertEquals(TransparentAddressKind.P2SH, snapshot.inputs.last().destination.kind)
                assertEquals("44".repeat(20), snapshot.inputs.last().destination.hashHex)
                assertEquals("55".repeat(20), snapshot.outputs.first().hashHex)
                assertEquals("66".repeat(20), snapshot.outputs.last().hashHex)
                assertTrue(snapshot.remainingMillis in 1L..90000L)
                val copy = review.unsignedBytes()
                assertArrayEquals(expected, copy)
                copy.fill(0)
                assertArrayEquals(expected, review.unsignedBytes())
            }
        }
    }

    @Test fun closedOwnerCannotReachRepeatedReplacement() {
        repeat(12) {
            val old = open()
            try {
                assertEquals(CoreStatus.BUSY, failure { open().close() })
                assertEquals(500L, old.snapshot().fee.value)
            } finally { old.close() }
            open().use { current ->
                assertEquals(CoreStatus.CANCELLED, failure { old.snapshot() })
                old.close()
                assertEquals(500L, current.snapshot().fee.value)
            }
        }
    }

    @Test fun inclusiveExpiryAndRollbackClearNativeOwner() {
        var now = 100L
        open { now }.use { review ->
            now = 90099
            assertEquals(1L, review.snapshot().remainingMillis)
            now = 90100
            assertEquals(CoreStatus.TIMED_OUT, failure { review.unsignedBytes() })
            open().use { assertEquals(500L, it.snapshot().fee.value) }
        }
        now = 100
        open { now }.use { review ->
            now = 99
            assertEquals(CoreStatus.CANCELLED, failure { review.snapshot() })
            open().use { assertEquals(500L, it.snapshot().fee.value) }
        }
    }
}
