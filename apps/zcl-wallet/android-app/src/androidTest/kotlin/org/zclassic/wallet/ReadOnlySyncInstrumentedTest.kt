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
                now.set(60005)
                assertEquals(ReadOnlySync.Freshness.UNVERIFIED, sync.snapshot().freshness)
                now.set(60006)
                assertEquals(ReadOnlySync.Freshness.STALE, sync.snapshot().freshness)
            }
            now.set(0)
            ReadOnlySync(address, ByteArray(32) { 1 }, now::get).use { fresh ->
                val attempt = fresh.begin(1)
                assertEquals(CoreStatus.CANCELLED, checkNotNull(old).reply(byteArrayOf()))
                assertNull(fresh.snapshot().report)
                assertTrue(fresh.snapshot().refreshing)
                now.set(1)
                assertEquals(CoreStatus.TIMED_OUT, attempt.reply(byteArrayOf()))
                assertFalse(fresh.snapshot().refreshing)
                assertNull(fresh.snapshot().report)
            }
        }
    }
}
