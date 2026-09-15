// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.os.Handler
import android.os.Looper
import android.os.Message
import android.os.Process
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicInteger
import java.util.concurrent.atomic.AtomicReference
import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith

/** Actual isolated-service connection, with a private Handler refusing only
 * the readiness handoff. No camera, decoded data, wallet or key is used. */
@RunWith(AndroidJUnit4::class)
class ScanReadyHandoffInstrumentedTest {
    private val instrumentation = InstrumentationRegistry.getInstrumentation()
    private fun field(name: String) = ScanDecodeClient::class.java.getDeclaredField(name).apply { isAccessible = true }

    private class PostingHandler(private val connectTimeout: Runnable, private val enqueue: Boolean,
                                 private val reject: Boolean) : Handler(Looper.getMainLooper()) {
        val posts = AtomicInteger()
        val arrived = CountDownLatch(1)
        val release = CountDownLatch(1)
        val notification = CountDownLatch(1)
        override fun sendMessageAtTime(message: Message, uptimeMillis: Long): Boolean {
            if (message.callback === connectTimeout) return super.sendMessageAtTime(message, uptimeMillis)
            val first = posts.incrementAndGet() == 1
            if (first && reject) {
                arrived.countDown()
                check(release.await(5, TimeUnit.SECONDS))
                if (enqueue) check(super.sendMessageAtTime(message, uptimeMillis))
                throw IllegalStateException("Public injected readiness handoff refusal")
            }
            val accepted = super.sendMessageAtTime(message, uptimeMillis)
            if (accepted && reject) notification.countDown()
            return accepted
        }
    }

    private fun onMain(action: () -> Unit) {
        val problem = AtomicReference<Throwable?>()
        instrumentation.runOnMainSync {
            try { action() } catch (failure: Throwable) { problem.set(failure) }
        }
        problem.get()?.let { throw it }
    }

    private fun connection(enqueue: Boolean, reject: Boolean) {
        val completed = CountDownLatch(1)
        val ready = AtomicInteger()
        val failed = AtomicInteger()
        val client = ScanDecodeClient(instrumentation.targetContext,
            { ready.incrementAndGet(); completed.countDown() },
            { failed.incrementAndGet(); completed.countDown() })
        val handler = PostingHandler(field("connectTimeout").get(client) as Runnable, enqueue, reject)
        try {
            instrumentation.runOnMainSync {
                field("main").set(client, handler)
                client.connect()
            }
            if (reject) {
                assertTrue("Isolated readiness did not reach the handoff", handler.arrived.await(20, TimeUnit.SECONDS))
                // Hold actual UI delivery until Binder has handled the thrown
                // post and queued its failure. No callback is removed or slept.
                onMain {
                    handler.release.countDown()
                    assertTrue("Failed handoff did not retire and report", handler.notification.await(5, TimeUnit.SECONDS))
                }
            }
            assertTrue("Connection produced no terminal callback", completed.await(20, TimeUnit.SECONDS))
            instrumentation.waitForIdleSync()
            assertTrue("Readiness did not come from an isolated process", client.decoderUid >= 0)
            assertNotEquals(Process.myUid(), client.decoderUid)
            assertEquals(if (reject) 0 else 1, ready.get())
            assertEquals(if (reject) 1 else 0, failed.get())
            assertEquals(!reject, client.isReady)
            assertEquals(if (reject) 2 else 1, handler.posts.get())
            if (reject) assertFalse((field("bound").get(client) as AtomicBoolean).get())
        } finally {
            handler.release.countDown()
            instrumentation.runOnMainSync {
                client.close()
                handler.removeCallbacksAndMessages(null)
            }
        }
    }

    @Test fun readinessPostExceptionRetiresTheActualServiceConnection() = connection(enqueue = false, reject = true)

    @Test fun queuedReadinessIsInertAfterExceptionalHandoff() = connection(enqueue = true, reject = true)

    @Test fun ordinaryReadinessStillDeliversOnce() = connection(enqueue = true, reject = false)
}
