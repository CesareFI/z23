// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import java.util.concurrent.atomic.AtomicLong
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.zclassic.wallet.core.CoreStatus
import org.zclassic.wallet.core.Network
import org.zclassic.wallet.core.ReadOnlySync
import org.zclassic.wallet.core.TransparentAddress
import org.zclassic.wallet.core.Zatoshi

/** Public local fixtures only. No endpoint, socket, wallet record or Keystore. */
@RunWith(AndroidJUnit4::class)
class ReadOnlySyncInstrumentedTest {
    @Test fun retiredReplyBurstCannotExpireOrPoisonTheCurrentAttempt() {
        val assets = InstrumentationRegistry.getInstrumentation().context.assets
        val frame = ByteArray(16384) { 42 } // Public bytes, never parsed for a retired token.
        for (network in Network.entries) for (history in listOf(false, true)) {
            val address = TransparentAddress.fromPublicKeyHash(ByteArray(20), network)
            val now = AtomicLong(100)
            val sync = if (history) ReadOnlySync.withHistory(address, ByteArray(32) { 1 }, now::get)
                else ReadOnlySync(address, ByteArray(32) { 1 }, now::get)
            sync.use {
                val old = sync.begin(100)
                assertEquals(CoreStatus.CANCELLED, old.fail())
                val current = sync.begin(100)
                assertTrue(current.request().isNotEmpty())
                val before = sync.snapshot()
                now.set(Long.MAX_VALUE)
                repeat(64) { assertEquals(CoreStatus.CANCELLED, old.reply(frame)) }
                now.set(100)
                assertEquals(before, sync.snapshot())
                assertTrue(frame.all { it == 42.toByte() })
                val name = if (network == Network.MAINNET) "mainnet" else "testnet"
                val version = assets.open("sync/$name-1.json").use { it.readBytes() }
                now.set(101)
                assertEquals(CoreStatus.OK, current.reply(version))
                assertTrue(current.request().isNotEmpty())
            }
        }
    }

    @Test fun signedPendingAmountsUseCheckedNativeFormatting() {
        val cases = mapOf(0L to "0", -7L to "-0.00000007", 7L to "+0.00000007",
            Zatoshi.MAX_VALUE to "+21000000", -Zatoshi.MAX_VALUE to "-21000000",
            Zatoshi.MAX_VALUE - 1 to "+20999999.99999999",
            1 - Zatoshi.MAX_VALUE to "-20999999.99999999")
        for ((value, expected) in cases) assertEquals(expected, Zatoshi.formatDelta(value))
        for (value in listOf(Long.MIN_VALUE, Long.MAX_VALUE,
                             Zatoshi.MAX_VALUE + 1, -Zatoshi.MAX_VALUE - 1)) {
            var refused = false
            try { Zatoshi.formatDelta(value) }
            catch (_: IllegalArgumentException) { refused = true }
            assertTrue(refused)
        }
        assertEquals("0.00000007", Zatoshi.of(7).format())
    }

    @Test fun bothNetworksPublishOnlyCompleteUnverifiedReportsAndRestartEmpty() {
        val assets = InstrumentationRegistry.getInstrumentation().context.assets
        for (network in Network.entries) {
            val address = TransparentAddress.fromPublicKeyHash(ByteArray(20), network)
            val name = if (network == Network.MAINNET) "mainnet" else "testnet"
            val now = AtomicLong(0)
            var old: ReadOnlySync.Attempt? = null
            ReadOnlySync(address, ByteArray(32) { 1 }, now::get).use { sync ->
                val attempt = sync.begin(100)
                old = attempt
                for (step in 1..6) {
                    now.set(step.toLong())
                    assertNull(sync.snapshot().report)
                    assertTrue(attempt.request().isNotEmpty())
                    val frame = assets.open("sync/$name-$step.json").use { it.readBytes() }
                    assertEquals(CoreStatus.OK, attempt.reply(frame))
                }
                val complete = sync.snapshot()
                assertEquals(ReadOnlySync.Freshness.UNVERIFIED, complete.freshness)
                val report = checkNotNull(complete.report)
                assertEquals(1000L, report.confirmed.value)
                assertEquals(-7L, report.pendingDelta)
                assertEquals(993L, report.total.value)
                assertFalse(complete.refreshing)
                assertEquals(60000L, complete.nextChangeDelayMillis)
                now.set(60005)
                assertEquals(ReadOnlySync.Freshness.UNVERIFIED, sync.snapshot().freshness)
                assertEquals(1L, sync.snapshot().nextChangeDelayMillis)
                now.set(60006)
                assertEquals(ReadOnlySync.Freshness.STALE, sync.snapshot().freshness)
                assertEquals(0L, sync.snapshot().nextChangeDelayMillis)
            }
            now.set(0)
            ReadOnlySync(address, ByteArray(32) { 1 }, now::get).use { fresh ->
                val attempt = fresh.begin(1)
                assertEquals(CoreStatus.CANCELLED, checkNotNull(old).reply(byteArrayOf()))
                assertNull(fresh.snapshot().report)
                assertTrue(fresh.snapshot().refreshing)
                assertEquals(1L, fresh.snapshot().nextChangeDelayMillis)
                now.set(1)
                assertEquals(CoreStatus.TIMED_OUT, attempt.reply(byteArrayOf()))
                assertFalse(fresh.snapshot().refreshing)
                assertEquals(0L, fresh.snapshot().nextChangeDelayMillis)
                assertNull(fresh.snapshot().report)
            }
        }
    }
}
