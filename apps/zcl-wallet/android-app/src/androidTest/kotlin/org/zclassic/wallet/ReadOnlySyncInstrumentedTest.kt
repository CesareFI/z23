// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
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

/** Public local fixtures only. No endpoint, socket, wallet record or Keystore. */
@RunWith(AndroidJUnit4::class)
class ReadOnlySyncInstrumentedTest {
    @Test fun bothNetworksPublishOnlyCompleteUnverifiedReportsAndRestartEmpty() {
        val assets = InstrumentationRegistry.getInstrumentation().context.assets
        for (network in Network.entries) {
            val address = TransparentAddress.fromPublicKeyHash(ByteArray(20), network)
            val name = if (network == Network.MAINNET) "mainnet" else "testnet"
            var old: ReadOnlySync.Attempt? = null
            ReadOnlySync(address, ByteArray(32) { 1 }).use { sync ->
                val attempt = sync.begin(0, 100)
                old = attempt
                for (step in 1..6) {
                    assertNull(sync.snapshot(step.toLong()).report)
                    assertTrue(attempt.request(step.toLong()).isNotEmpty())
                    val frame = assets.open("sync/$name-$step.json").use { it.readBytes() }
                    assertEquals(CoreStatus.OK, attempt.reply(step.toLong(), frame))
                }
                val complete = sync.snapshot(6)
                assertEquals(ReadOnlySync.Freshness.UNVERIFIED, complete.freshness)
                val report = checkNotNull(complete.report)
                assertEquals(1000L, report.confirmed.value)
                assertEquals(-7L, report.pendingDelta)
                assertEquals(993L, report.total.value)
                assertFalse(complete.refreshing)
                assertEquals(ReadOnlySync.Freshness.UNVERIFIED, sync.snapshot(60005).freshness)
                assertEquals(ReadOnlySync.Freshness.STALE, sync.snapshot(60006).freshness)
            }
            ReadOnlySync(address, ByteArray(32) { 1 }).use { fresh ->
                val attempt = fresh.begin(0, 1)
                assertEquals(CoreStatus.CANCELLED, checkNotNull(old).reply(0, byteArrayOf()))
                assertNull(fresh.snapshot(0).report)
                assertTrue(fresh.snapshot(0).refreshing)
                assertEquals(CoreStatus.TIMED_OUT, attempt.reply(1, byteArrayOf()))
                assertFalse(fresh.snapshot(1).refreshing)
                assertNull(fresh.snapshot(1).report)
            }
        }
    }
}
