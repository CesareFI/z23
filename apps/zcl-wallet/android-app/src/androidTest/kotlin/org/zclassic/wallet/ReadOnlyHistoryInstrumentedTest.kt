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
import org.zclassic.wallet.core.ReadOnlySync
import org.zclassic.wallet.core.TransparentAddress

/** Public local protocol fixtures only. No wallet, Keystore, endpoint or funds. */
@RunWith(AndroidJUnit4::class)
class ReadOnlyHistoryInstrumentedTest {
    private fun fixture(network: Network, step: Int): ByteArray {
        val name = if (network == Network.MAINNET) "mainnet" else "testnet"
        val original = InstrumentationRegistry.getInstrumentation().context.assets
            .open("sync/$name-${minOf(step, 6)}.json").use { it.readBytes() }
        return if (step == 7) original.toString(Charsets.US_ASCII)
            .replaceFirst("\"id\":6", "\"id\":7").toByteArray(Charsets.US_ASCII) else original
    }

    private fun history(): ByteArray {
        val entries = (0 until 16).joinToString(",") { index ->
            "{\"tx_hash\":\"${"f".repeat(56)}${index.toString(16).padStart(8, '0')}\",\"height\":${-(index % 2)}}"
        }
        return "{\"id\":6,\"result\":[$entries]}".toByteArray(Charsets.US_ASCII)
    }

    @Test fun maximumHistoryIsAtomicAndClosedOwnerCannotReachReplacement() {
        for (network in Network.entries) {
            val address = TransparentAddress.fromPublicKeyHash(ByteArray(20), network)
            val first = ReadOnlySync.withHistory(address, ByteArray(32) { 1 }, SystemClock::elapsedRealtime)
            val previous = first.use { sync ->
                val attempt = sync.begin()
                for (step in 1..7) {
                    assertNull(sync.snapshot().report)
                    assertTrue(attempt.request().isNotEmpty())
                    assertEquals(CoreStatus.OK, attempt.reply(if (step == 6) history() else fixture(network, step)))
                }
                val snapshot = sync.snapshot()
                assertEquals(ReadOnlySync.Freshness.UNVERIFIED, snapshot.freshness)
                val report = checkNotNull(snapshot.report)
                assertEquals(993L, report.total.value)
                val entries = checkNotNull(report.history)
                assertEquals(16, entries.size)
                assertEquals("f".repeat(56) + "0000000f", entries.last().transactionId)
                assertEquals(-1L, entries.last().reportedHeight)
                attempt
            }
            ReadOnlySync.withHistory(address, ByteArray(32) { 2 }, SystemClock::elapsedRealtime).use { sync ->
                val current = sync.begin()
                assertEquals(CoreStatus.CANCELLED, previous.reply(history()))
                assertEquals(CoreStatus.CANCELLED, previous.fail())
                assertNull(sync.snapshot().report)
                assertTrue(sync.snapshot().refreshing)
                assertEquals(CoreStatus.CANCELLED, current.fail())
            }
        }
    }
}
