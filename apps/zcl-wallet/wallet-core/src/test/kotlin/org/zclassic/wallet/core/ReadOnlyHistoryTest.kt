// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet.core

import java.util.concurrent.atomic.AtomicLong
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertFalse
import kotlin.test.assertNull
import kotlin.test.assertTrue

class ReadOnlyHistoryTest {
    private val now = AtomicLong(0)
    private fun address(network: Network) = TransparentAddress.fromPublicKeyHash(ByteArray(20), network)
    private fun owner(network: Network = Network.MAINNET) =
        ReadOnlySync.withHistory(address(network), ByteArray(32) { 1 }, now::get)

    private fun fixture(network: Network, step: Int): ByteArray {
        val name = if (network == Network.MAINNET) "mainnet" else "testnet"
        val original = checkNotNull(javaClass.getResourceAsStream("/sync/$name-${minOf(step, 6)}.json"))
            .use { it.readBytes() }
        return if (step == 7) original.toString(Charsets.US_ASCII)
            .replaceFirst("\"id\":6", "\"id\":7").toByteArray(Charsets.US_ASCII) else original
    }

    private fun history(count: Int = 16, duplicate: Boolean = false, height: Int? = null): ByteArray {
        val entries = (0 until count).joinToString(",") { index ->
            val suffix = (if (duplicate) 0 else index).toString(16).padStart(8, '0')
            "{\"tx_hash\":\"${"f".repeat(56)}$suffix\",\"height\":${height ?: -(index % 2)}}"
        }
        return "{\"id\":6,\"result\":[$entries]}".toByteArray(Charsets.US_ASCII)
    }

    private fun throughBalance(sync: ReadOnlySync, network: Network): ReadOnlySync.Attempt {
        val attempt = sync.begin()
        for (step in 1..5) {
            assertNull(sync.snapshot().report)
            assertTrue(attempt.request().isNotEmpty())
            assertEquals(CoreStatus.OK, attempt.reply(fixture(network, step)))
        }
        assertTrue(attempt.request().toString(Charsets.US_ASCII).contains("scripthash.get_history"))
        return attempt
    }

    @Test fun completeHistoryUsesOneSnapshotAndPreservesUnsignedHashWords() {
        for (network in Network.entries) {
            now.set(0)
            owner(network).use { sync ->
                val attempt = throughBalance(sync, network)
                assertEquals(CoreStatus.OK, attempt.reply(history()))
                assertNull(sync.snapshot().report)
                assertTrue(attempt.request().toString(Charsets.US_ASCII).contains("headers.subscribe"))
                assertEquals(CoreStatus.OK, attempt.reply(fixture(network, 7)))
                val snapshot = sync.snapshot()
                assertEquals(ReadOnlySync.Freshness.UNVERIFIED, snapshot.freshness)
                val report = checkNotNull(snapshot.report)
                assertEquals(993L, report.total.value)
                val entries = checkNotNull(report.history)
                assertEquals(16, entries.size)
                entries.forEachIndexed { index, entry ->
                    assertEquals("f".repeat(56) + index.toString(16).padStart(8, '0'), entry.transactionId)
                    assertEquals(-(index % 2).toLong(), entry.reportedHeight)
                }
                now.set(60_000)
                assertEquals(ReadOnlySync.Freshness.STALE, sync.snapshot().freshness)
                val retry = sync.begin()
                assertEquals(CoreStatus.IO_FAILURE, retry.fail(CoreStatus.IO_FAILURE))
                assertEquals(report, sync.snapshot().report)
                now.set(59_999)
                assertNull(sync.snapshot().report)
            }
            owner(network).use { assertNull(it.snapshot().report) }
        }
    }

    @Test fun emptyHistoryDiffersFromDefaultBalanceAndMalformedHistoryCannotPublish() {
        owner().use { sync ->
            val attempt = throughBalance(sync, Network.MAINNET)
            assertEquals(CoreStatus.OK, attempt.reply(history(0)))
            attempt.request()
            assertEquals(CoreStatus.OK, attempt.reply(fixture(Network.MAINNET, 7)))
            assertEquals(emptyList(), checkNotNull(sync.snapshot().report).history)
        }
        ReadOnlySync(address(Network.MAINNET), ByteArray(32)) { now.get() }.use { sync ->
            val attempt = sync.begin()
            for (step in 1..6) {
                assertFalse(attempt.request().toString(Charsets.US_ASCII).contains("get_history"))
                assertEquals(CoreStatus.OK, attempt.reply(fixture(Network.MAINNET, step)))
            }
            assertNull(checkNotNull(sync.snapshot().report).history)
        }
        val cases = listOf(history(17) to CoreStatus.RESOURCE_EXHAUSTED,
            history(2, duplicate = true) to CoreStatus.INVALID_ENCODING,
            history(1, height = 1) to CoreStatus.IO_UNCERTAIN,
            history(1, height = -2) to CoreStatus.OUT_OF_RANGE)
        for ((frame, expected) in cases) owner().use { sync ->
            val attempt = throughBalance(sync, Network.MAINNET)
            assertEquals(expected, attempt.reply(frame))
            assertNull(sync.snapshot().report)
            assertEquals(expected, sync.snapshot().lastFault)
        }
    }

    @Test fun historyModeSharesBoundedPoolAndClosedCallbacksCannotReachNewMode() {
        val first = owner()
        val oldAttempt = first.begin()
        first.close()
        ReadOnlySync(address(Network.MAINNET), ByteArray(32), now::get).use { replacement ->
            replacement.begin()
            assertEquals(CoreStatus.CANCELLED, oldAttempt.reply(history()))
            assertEquals(CoreStatus.CANCELLED, oldAttempt.fail())
            assertTrue(replacement.snapshot().refreshing)
            assertNull(replacement.snapshot().report)
        }
        val owners = ArrayList<ReadOnlySync>()
        try {
            repeat(4) { owners.add(owner()) }
            assertEquals(CoreStatus.RESOURCE_EXHAUSTED,
                assertFailsWith<ReadOnlySyncFailure> { owner() }.status)
        } finally { owners.forEach { it.close() } }
        owner().use { sync ->
            assertEquals(CoreStatus.OUT_OF_RANGE,
                assertFailsWith<ReadOnlySyncFailure> { sync.begin(firstId = 0xffff_ffffL - 5) }.status)
            assertFalse(sync.snapshot().refreshing)
            sync.begin(firstId = 0xffff_ffffL - 6).fail()
        }
    }

    @Test fun directJniHistoryArgumentsAndClosedPacketsFailClosed() {
        val text = address(Network.MAINNET).encoded.toByteArray(Charsets.US_ASCII)
        assertTrue(NativeCore.openHistorySyncOwner(text, Network.MAINNET.nativeId, ByteArray(33)) < 0)
        assertTrue(NativeCore.openHistorySyncOwner(ByteArray(36), Network.MAINNET.nativeId, ByteArray(32)) < 0)
        assertTrue(NativeCore.openHistorySyncOwner(text, -1, ByteArray(32)) < 0)
        val id = NativeCore.openHistorySyncOwner(text, Network.MAINNET.nativeId, ByteArray(32))
        assertTrue(id > 0)
        try {
            val negative = checkNotNull(NativeCore.syncHistorySnapshot(id, -1))
            assertEquals(12, negative.size)
            assertEquals(CoreStatus.OUT_OF_RANGE.code.toLong(), negative[0])
            val unavailable = checkNotNull(NativeCore.syncHistorySnapshot(id, 0))
            assertEquals(12, unavailable.size)
            assertEquals(0L, unavailable[10])
        } finally { assertEquals(CoreStatus.OK.code, NativeCore.closeSyncOwner(id)) }
        assertEquals(CoreStatus.CANCELLED.code.toLong(), checkNotNull(NativeCore.syncHistorySnapshot(id, 0))[0])
    }
}
