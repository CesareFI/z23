// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.view.View
import android.view.ViewGroup
import android.widget.LinearLayout
import androidx.test.core.app.ActivityScenario
import androidx.test.ext.junit.runners.AndroidJUnit4
import java.io.File
import java.nio.file.Files
import java.util.concurrent.ConcurrentLinkedQueue
import java.util.concurrent.CountDownLatch
import java.util.concurrent.Executor
import java.util.concurrent.ThreadPoolExecutor
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicReference
import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith
import org.zclassic.wallet.core.WalletStorage

/** Public invalid marker text on the storage-free debug host. The worker is
 * held before any wallet operation; these cases never authenticate or derive
 * keys, open wallet storage, or use a recovery seed. */
@RunWith(AndroidJUnit4::class)
class RecoverySubmissionInstrumentedTest {
    private class Screen(host: WalletDisplayFixtureActivity) : AutoCloseable {
        val screens = WalletScreens(host)
        val content = WalletScreens::class.java.getDeclaredField("content").apply { isAccessible = true }
            .get(screens) as LinearLayout
        var transferred: CharArray? = null
        private var original: CharArray? = null

        fun prepare(confirming: Boolean = false, receive: (CharArray) -> Unit) {
            screens.enterRecovery(confirming, false, { words ->
                transferred = words
                receive(words)
            }, { error("Submission must not cancel the screen") })
            val input = content.findViewById<RecoveryInputView>(R.id.recovery_input)
            original = RecoveryInputView::class.java.getDeclaredField("characters").apply { isAccessible = true }
                .get(input) as CharArray
            input.append('a'); input.append('b'); input.append('c')
        }

        fun submit() { assertTrue(content.findViewById<View>(R.id.save_wallet).performClick()) }

        fun assertOriginalCleared() {
            assertTrue("Input retained characters after transfer", checkNotNull(original).all { it == '\u0000' })
        }

        fun assertTransferCleared() {
            assertOriginalCleared()
            assertTrue("Transferred phrase outlived refusal/cancellation",
                checkNotNull(transferred).all { it == '\u0000' })
        }

        override fun close() {
            content.setOnHierarchyChangeListener(null)
            transferred?.fill('\u0000')
            screens.clearSecrets()
        }
    }

    private class Worker(host: WalletDisplayFixtureActivity) : AutoCloseable {
        private val parent = Files.createTempDirectory(host.cacheDir.toPath(), "public-recovery-submit-").toFile()
        private val directory = File(parent, "untouched")
        val callbacks = ConcurrentLinkedQueue<Runnable>()
        val session = WalletPlatformSession(host, WalletStorage(directory.absolutePath), Executor { callbacks.add(it) })
        private val work = WalletPlatformSession::class.java.getDeclaredField("work").apply { isAccessible = true }
            .get(session) as OwnedExecutor
        private val release = CountDownLatch(1)
        private val workerFailure = AtomicReference<Throwable?>()
        private var backend: ThreadPoolExecutor? = null
        var problem: WalletProblem? = null

        fun hold() {
            val entered = CountDownLatch(1)
            assertTrue(work.submit {
                entered.countDown()
                try { check(release.await(10, TimeUnit.SECONDS)) { "Public worker gate expired" } }
                catch (failure: Throwable) { workerFailure.set(failure) }
            })
            assertTrue(entered.await(5, TimeUnit.SECONDS))
            backend = OwnedExecutor::class.java.getDeclaredField("executor").apply { isAccessible = true }
                .get(work) as ThreadPoolExecutor
        }

        fun saturate() {
            repeat(4) { assertTrue(work.submit { error("Cancelled filler unexpectedly ran") }) }
        }

        fun submit(words: CharArray, confirming: Boolean) {
            val invalid = { error("Held recovery operation unexpectedly ran") }
            if (confirming) session.confirmCreation(words, invalid,
                { error("No wallet may complete") }, { problem = it })
            else session.restore(words, invalid, { error("No wallet may complete") }, { problem = it })
        }

        override fun close() {
            try { session.close() } finally { release.countDown() }
            backend?.let { assertTrue(it.awaitTermination(5, TimeUnit.SECONDS)) }
            workerFailure.get()?.let { throw it }
            assertFalse("Submission fixture touched storage", directory.exists())
            assertTrue("Unexpected public fixture files", parent.delete())
        }
    }

    private fun withScreen(action: (WalletDisplayFixtureActivity, Screen) -> Unit) {
        ActivityScenario.launch(WalletDisplayFixtureActivity::class.java).use { scenario ->
            val failure = AtomicReference<Throwable?>()
            scenario.onActivity { host ->
                try { Screen(host).use { action(host, it) } }
                catch (problem: Throwable) { failure.set(problem) }
            }
            failure.get()?.let { throw it }
        }
    }

    private fun failedSubmission(problem: Throwable) = withScreen { _, screen ->
        screen.prepare { throw problem }
        assertSame(problem, assertThrows(problem.javaClass) { screen.submit() })
        screen.assertTransferCleared()
    }

    @Test fun submissionExceptionErasesTheTransferredPhrase() =
        failedSubmission(IllegalStateException("Public submission refusal"))

    @Test fun submissionErrorErasesTheTransferredPhrase() =
        failedSubmission(OutOfMemoryError("Public synthetic submission allocation failure"))

    @Test fun waitingScreenFailureErasesInputBeforeWorkerHandoff() = withScreen { _, screen ->
        val problem = OutOfMemoryError("Public waiting screen failure")
        var reachedWorker = false
        screen.prepare {
            screen.screens.waiting()
            reachedWorker = true
        }
        screen.content.setOnHierarchyChangeListener(object : ViewGroup.OnHierarchyChangeListener {
            override fun onChildViewAdded(parent: View?, child: View?) { throw problem }
            override fun onChildViewRemoved(parent: View?, child: View?) = Unit
        })
        assertSame(problem, assertThrows(OutOfMemoryError::class.java) { screen.submit() })
        assertFalse(reachedWorker)
        screen.assertTransferCleared()
    }

    @Test fun successfulHandoffRetiresInputAndTransfersTheExactCharacters() = withScreen { _, screen ->
        screen.prepare { assertArrayEquals(charArrayOf('a', 'b', 'c'), it) }
        screen.submit()
        screen.assertOriginalCleared()
        assertArrayEquals(charArrayOf('a', 'b', 'c'), screen.transferred)
    }

    @Test fun queuedCancellationErasesBothConfirmationAndRestorationInput() {
        for (confirming in listOf(false, true)) withScreen { host, screen ->
            Worker(host).use { worker ->
                worker.hold()
                screen.prepare(confirming) { worker.submit(it, confirming) }
                screen.submit()
                screen.assertOriginalCleared()
                assertArrayEquals(charArrayOf('a', 'b', 'c'), screen.transferred)
                worker.session.close()
                screen.assertTransferCleared()
                assertTrue(worker.callbacks.isEmpty())
                assertNull(worker.problem)
            }
        }
    }

    @Test fun queueRefusalErasesBothInputsBeforeReportingResourceFailure() {
        for (confirming in listOf(false, true)) withScreen { host, screen ->
            Worker(host).use { worker ->
                worker.hold()
                worker.saturate()
                screen.prepare(confirming) { worker.submit(it, confirming) }
                screen.submit()
                screen.assertTransferCleared()
                assertNull(worker.problem)
                assertEquals(1, worker.callbacks.size)
                checkNotNull(worker.callbacks.poll()).run()
                assertEquals(WalletProblem.RESOURCES, worker.problem)
            }
        }
    }
}
