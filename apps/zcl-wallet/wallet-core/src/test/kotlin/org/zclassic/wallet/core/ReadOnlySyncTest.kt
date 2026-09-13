// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet.core

import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicReference
import java.util.concurrent.atomic.AtomicLong
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertFalse
import kotlin.test.assertNull
import kotlin.test.assertTrue

class ReadOnlySyncTest {
    private val address = TransparentAddress.fromPublicKeyHash(ByteArray(20), Network.MAINNET)
    private val now = AtomicLong(0)
    private fun owner(): ReadOnlySync = ReadOnlySync(address, ByteArray(32) { 1 }, now::get)
    private fun failure(status: CoreStatus, operation: () -> Unit) {
        assertEquals(status, assertFailsWith<ReadOnlySyncFailure> { operation() }.status)
    }

    private fun fixture(network: Network, step: Int): ByteArray {
        val name = if (network == Network.MAINNET) "mainnet" else "testnet"
        return checkNotNull(javaClass.getResourceAsStream("/sync/$name-$step.json")).use { it.readBytes() }
    }

    @Test fun completeFixturePublishesSignedDeltaAndBecomesStaleAtExactBoundary() {
        for (network in Network.entries) {
            val receiving = TransparentAddress.fromPublicKeyHash(ByteArray(20), network)
            ReadOnlySync(receiving, ByteArray(32) { 1 }, now::get).use { sync ->
                now.set(100)
                val attempt = sync.begin(100)
                for (step in 1..6) {
                    now.set(100 + step.toLong())
                    assertNull(sync.snapshot().report)
                    assertTrue(attempt.request().last() == '\n'.code.toByte())
                    assertEquals(CoreStatus.OK, attempt.reply(fixture(network, step)))
                }
                val complete = sync.snapshot()
                assertEquals(ReadOnlySync.Freshness.UNVERIFIED, complete.freshness)
                assertFalse(complete.refreshing)
                val report = checkNotNull(complete.report)
                assertEquals(1000L, report.confirmed.value)
                assertEquals(-7L, report.pendingDelta)
                assertEquals(993L, report.total.value)
                assertEquals(0L, report.height)
                now.set(60105)
                assertEquals(ReadOnlySync.Freshness.UNVERIFIED, sync.snapshot().freshness)
                now.set(60106)
                assertEquals(ReadOnlySync.Freshness.STALE, sync.snapshot().freshness)
                val refresh = sync.begin(10)
                assertEquals(CoreStatus.IO_FAILURE, refresh.fail(CoreStatus.IO_FAILURE))
                assertEquals(report, sync.snapshot().report)
                assertEquals(ReadOnlySync.Freshness.STALE, sync.snapshot().freshness)
                now.set(60105)
                assertNull(sync.snapshot().report) // Clock rollback drops the cached report.
            }
            now.set(0)
            ReadOnlySync(receiving, ByteArray(32) { 1 }, now::get).use { assertNull(it.snapshot().report) }
        }
    }

    @Test fun initialStateIsUnavailableAndClosedAttemptsCannotReachReplacement() {
        val first = owner()
        val oldAttempt = first.begin(100)
        first.close()
        first.close()
        owner().use { second ->
            val attempt = second.begin(100)
            failure(CoreStatus.CANCELLED) { oldAttempt.request() }
            assertEquals(CoreStatus.CANCELLED, oldAttempt.reply(byteArrayOf()))
            assertEquals(CoreStatus.CANCELLED, oldAttempt.fail())
            val snapshot = second.snapshot()
            assertEquals(ReadOnlySync.Freshness.UNAVAILABLE, snapshot.freshness)
            assertNull(snapshot.report)
            assertTrue(snapshot.refreshing)
            assertEquals(CoreStatus.OK, snapshot.lastFault)
            assertTrue(attempt.request().toString(Charsets.US_ASCII).contains("server.version"))
        }
    }

    @Test fun deadlinesAndWrongRepliesFailClosedAndExplicitRetryWorks() = owner().use { sync ->
        now.set(100)
        val timed = sync.begin(10)
        now.set(109)
        timed.request()
        now.set(110)
        assertEquals(CoreStatus.TIMED_OUT, timed.reply(
            "{\"id\":1,\"result\":[\"fixture\",\"1.2\"]}".toByteArray()))
        assertFalse(sync.snapshot().refreshing)
        assertNull(sync.snapshot().report)
        val retry = sync.begin(10)
        now.set(Long.MAX_VALUE)
        failure(CoreStatus.CANCELLED) { timed.request() }
        now.set(110)
        retry.request()
        now.set(111)
        assertEquals(CoreStatus.INVALID_ENCODING, retry.reply(
            "{\"id\":2,\"result\":[\"fixture\",\"1.2\"]}".toByteArray()))
        assertEquals(CoreStatus.INVALID_ENCODING, sync.snapshot().lastFault)
        assertEquals(CoreStatus.CANCELLED, retry.fail())
        val next = sync.begin(10)
        next.request()
        assertEquals(CoreStatus.OK, next.reply(
            "{\"id\":1,\"result\":[\"fixture\",\"1.2\"]}".toByteArray()))
        assertTrue(next.request().toString(Charsets.US_ASCII).contains("server.features"))
    }

    @Test fun oversizedRepliesAbortButDoNotExposePartialBalance() = owner().use { sync ->
        val attempt = sync.begin()
        attempt.request()
        assertEquals(CoreStatus.OUT_OF_RANGE, attempt.reply(ByteArray(16385)))
        val snapshot = sync.snapshot()
        assertEquals(CoreStatus.OUT_OF_RANGE, snapshot.lastFault)
        assertNull(snapshot.report)
        assertFalse(snapshot.refreshing)
    }

    @Test fun signedBoundsAndUnknownStatusesCannotEnterCState() = owner().use { sync ->
        for (time in listOf(-1L, Long.MIN_VALUE, Long.MAX_VALUE)) {
            now.set(time)
            failure(CoreStatus.OUT_OF_RANGE) { sync.begin(1) }
        }
        now.set(0)
        for (timeout in listOf(-1L, 0L, 30001L, Long.MAX_VALUE))
            failure(CoreStatus.OUT_OF_RANGE) { sync.begin(timeout) }
        for (id in listOf(-1L, 0L, 4294967291L, Long.MAX_VALUE))
            failure(CoreStatus.OUT_OF_RANGE) { sync.begin(1, id) }
        val attempt = sync.begin(1, 4294967290L)
        assertEquals(CoreStatus.INVALID_ARGUMENT, attempt.fail(CoreStatus.OK))
        assertTrue(sync.snapshot().refreshing)
        now.set(-1)
        assertEquals(CoreStatus.OUT_OF_RANGE, attempt.reply(byteArrayOf()))
        assertEquals(CoreStatus.CANCELLED, attempt.fail())
        failure(CoreStatus.OUT_OF_RANGE) { sync.snapshot() }
    }

    @Test fun nativeHandlesHaveBoundsAndFullRegistryRecoversAfterClose() {
        val encoded = address.encoded.toByteArray(Charsets.US_ASCII)
        val handles = ArrayList<Long>()
        try {
            assertEquals(-CoreStatus.OUT_OF_RANGE.code.toLong(), NativeCore.openSyncOwner(encoded, 0, ByteArray(33)))
            assertEquals(-CoreStatus.INVALID_ARGUMENT.code.toLong(), NativeCore.openSyncOwner(encoded, 0, ByteArray(31)))
            repeat(4) {
                val id = NativeCore.openSyncOwner(encoded, 0, ByteArray(32))
                assertTrue(id > 0)
                handles.add(id)
            }
            assertEquals(-CoreStatus.RESOURCE_EXHAUSTED.code.toLong(), NativeCore.openSyncOwner(encoded, 0, ByteArray(32)))
            val old = handles.removeAt(0)
            assertEquals(CoreStatus.OK.code, NativeCore.closeSyncOwner(old))
            val fresh = NativeCore.openSyncOwner(encoded, 0, ByteArray(32))
            assertTrue(fresh > old)
            handles.add(fresh)
            for (id in listOf(old, 0L, -1L, Long.MIN_VALUE, Long.MAX_VALUE)) {
                assertEquals(CoreStatus.CANCELLED.code, NativeCore.closeSyncOwner(id))
                assertEquals(CoreStatus.CANCELLED.code.toLong(), checkNotNull(NativeCore.syncSnapshot(id, 0))[0])
            }
        } finally {
            for (id in handles) assertEquals(CoreStatus.OK.code, NativeCore.closeSyncOwner(id))
        }
        owner().use { assertNull(it.snapshot().report) }
    }

    @Test fun sourceIdentityIsCopiedAtEachManagedBoundary() {
        val source = ByteArray(32) { 1 }
        ReadOnlySync(address, source, now::get).use { sync ->
            source.fill(2)
            val returned = sync.sourceIdentity()
            returned.fill(3)
            assertContentEquals(ByteArray(32) { 1 }, sync.sourceIdentity())
        }
    }

    @Test fun closeAndConcurrentCallbacksSerializeWithoutNativePointerExposure() {
        val sync = owner()
        val attempt = sync.begin(100)
        now.set(1)
        val start = CountDownLatch(1)
        val done = CountDownLatch(2)
        val problem = AtomicReference<Throwable>()
        val workers = List(2) {
            Thread {
                try {
                    check(start.await(5, TimeUnit.SECONDS))
                    repeat(128) {
                        val result = attempt.reply("{}".toByteArray())
                        check(result == CoreStatus.INVALID_ENCODING || result == CoreStatus.CANCELLED)
                    }
                } catch (failure: Throwable) { problem.set(failure) }
                finally { done.countDown() }
            }.apply { start() }
        }
        try {
            start.countDown()
            sync.close()
            assertTrue(done.await(5, TimeUnit.SECONDS))
            assertNull(problem.get())
        } finally {
            start.countDown()
            workers.forEach { it.join(5000) }
            sync.close()
        }
        owner().use { assertTrue(it.begin().request().isNotEmpty()) }
    }

    @Test fun liveOperationsSampleClockInsideOwnerSerialization() {
        val ticks = AtomicLong(0)
        lateinit var sync: ReadOnlySync
        sync = ReadOnlySync(address, ByteArray(32)) {
            assertTrue(Thread.holdsLock(sync), "Clock sampling must follow owner-lock acquisition")
            ticks.incrementAndGet()
        }
        sync.use {
            assertNull(it.snapshot().report)
            val attempt = it.begin()
            assertTrue(attempt.request().isNotEmpty())
            assertEquals(CoreStatus.OK, attempt.reply(fixture(Network.MAINNET, 1)))
            assertEquals(CoreStatus.OK, it.snapshot().lastFault)
            assertTrue(ticks.get() >= 5)
        }
        val sampled = ticks.get()
        failure(CoreStatus.CANCELLED) { sync.snapshot() }
        assertEquals(sampled, ticks.get()) // Closed owners do not consult the clock.
    }

    @Test fun directJniCallbacksCannotFindAClosedOwnerDuringSlotReuse() {
        val encoded = address.encoded.toByteArray(Charsets.US_ASCII)
        val old = NativeCore.openSyncOwner(encoded, 0, ByteArray(32))
        assertTrue(old > 0)
        val start = CountDownLatch(1)
        val done = CountDownLatch(2)
        val problem = AtomicReference<Throwable>()
        var replacement = 0L
        val workers = List(2) {
            Thread {
                try {
                    check(start.await(5, TimeUnit.SECONDS))
                    repeat(128) {
                        val snapshot = checkNotNull(NativeCore.syncSnapshot(old, 0))
                        check(snapshot[0] == CoreStatus.OK.code.toLong() || snapshot[0] == CoreStatus.CANCELLED.code.toLong())
                    }
                } catch (failure: Throwable) { problem.set(failure) }
                finally { done.countDown() }
            }.apply { start() }
        }
        try {
            start.countDown()
            assertEquals(CoreStatus.OK.code, NativeCore.closeSyncOwner(old))
            replacement = NativeCore.openSyncOwner(encoded, 0, ByteArray(32))
            assertTrue(replacement > old)
            assertTrue(done.await(5, TimeUnit.SECONDS))
            assertNull(problem.get())
            assertEquals(-CoreStatus.CANCELLED.code.toLong(), NativeCore.beginSyncAttempt(old, 0, 1, 1))
            assertEquals(1L, NativeCore.beginSyncAttempt(replacement, 0, 1, 1))
        } finally {
            start.countDown()
            workers.forEach { it.join(5000) }
            if (replacement > 0) assertEquals(CoreStatus.OK.code, NativeCore.closeSyncOwner(replacement))
            // Covers assertion failure before the first close without replacing anything.
            val status = NativeCore.closeSyncOwner(old)
            assertTrue(status == CoreStatus.OK.code || status == CoreStatus.CANCELLED.code)
        }
    }
}
