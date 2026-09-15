// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.text.Editable
import android.text.TextWatcher
import android.view.View
import android.view.ViewGroup
import android.widget.LinearLayout
import androidx.test.core.app.ActivityScenario
import androidx.test.ext.junit.runners.AndroidJUnit4
import java.util.concurrent.atomic.AtomicReference
import org.junit.Assert.assertEquals
import org.junit.Assert.assertSame
import org.junit.Assert.assertThrows
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

/** Public marker text on the existing storage-free debug host. No keys/authentication. */
@RunWith(AndroidJUnit4::class)
class BackupScreenFailureInstrumentedTest {
    private fun withScreen(action: (WalletScreens, LinearLayout) -> Unit) {
        ActivityScenario.launch(WalletDisplayFixtureActivity::class.java).use { scenario ->
            val failure = AtomicReference<Throwable>()
            scenario.onActivity { activity ->
                val screens = WalletScreens(activity)
                val content = WalletScreens::class.java.getDeclaredField("content").apply {
                    isAccessible = true
                }.get(screens) as LinearLayout
                try { action(screens, content) }
                catch (problem: Throwable) { failure.set(problem) }
                finally {
                    content.setOnHierarchyChangeListener(null)
                    screens.clearSecrets()
                }
            }
            failure.get()?.let { throw it }
        }
    }

    private class ClearFailure(private val problem: Throwable) : TextWatcher {
        var armed = false
        override fun beforeTextChanged(text: CharSequence?, start: Int, count: Int, after: Int) {
            if (armed && after == 0) throw problem
        }
        override fun onTextChanged(text: CharSequence?, start: Int, before: Int, count: Int) = Unit
        override fun afterTextChanged(text: Editable?) = Unit
    }

    @Test fun firstControlFailureConcealsDeliveredWords() = failedControl(R.id.backup_written, false)
    @Test fun secondControlFailureConcealsDeliveredWords() = failedControl(R.id.cancel_setup, false)
    @Test fun failedCleanupConcealsTheFrameworkCopyAndPreservesBothFailures() =
        failedControl(R.id.backup_written, true)

    @Test fun earlyLayoutFailureClearsIncomingWordsBeforeAViewOwnsThem() = withScreen { screens, content ->
        val words = charArrayOf('a', 'b', 'c')
        val problem = OutOfMemoryError("Public synthetic backup layout failure")
        content.setOnHierarchyChangeListener(object : ViewGroup.OnHierarchyChangeListener {
            override fun onChildViewAdded(parent: View?, child: View?) { throw problem }
            override fun onChildViewRemoved(parent: View?, child: View?) = Unit
        })
        try {
            assertSame(problem, assertThrows(OutOfMemoryError::class.java) { screens.backup(words, {}, {}) })
            assertTrue("Incoming array was stranded before view ownership", words.all { it == '\u0000' })
            assertEquals(null, content.findViewById<RecoveryWordsView>(R.id.recovery_words))
        } finally { words.fill('\u0000') }
    }

    private fun failedControl(identifier: Int, failClear: Boolean) = withScreen { screens, content ->
        val words = charArrayOf('a', 'b', 'c')
        val problem = OutOfMemoryError("Public synthetic backup control failure")
        val cleanup = IllegalStateException("Public synthetic backup cleanup failure")
        val listener = ClearFailure(cleanup)
        val delivery = RecoveryPhraseDelivery { it.run() }
        var display: RecoveryWordsView? = null
        content.setOnHierarchyChangeListener(object : ViewGroup.OnHierarchyChangeListener {
            override fun onChildViewAdded(parent: View?, child: View?) {
                if (child is RecoveryWordsView) {
                    display = child
                    // Watching before setText exercises the framework's
                    // separate text buffer, as in the concealment fixtures.
                    if (failClear) child.addTextChangedListener(listener)
                    return
                }
                if (child?.id != identifier) return
                assertEquals("abc", checkNotNull(display).text.toString())
                listener.armed = true
                throw problem
            }
            override fun onChildViewRemoved(parent: View?, child: View?) = Unit
        })
        try {
            assertSame(problem, assertThrows(OutOfMemoryError::class.java) {
                delivery.post(words) { screens.backup(it, {}, {}) }
            })
            assertTrue(words.all { it == '\u0000' })
            val rendered = checkNotNull(display)
            assertEquals("Incomplete backup screen remained visible", View.INVISIBLE, rendered.visibility)
            assertEquals(if (failClear) "abc" else "", rendered.text.toString())
            assertEquals(if (failClear) listOf(cleanup) else emptyList<Throwable>(), problem.suppressed.toList())
            rendered.removeTextChangedListener(listener)
            content.setOnHierarchyChangeListener(null)
            val retry = charArrayOf('z')
            try {
                screens.backup(retry, {}, {})
                assertEquals(View.VISIBLE, content.findViewById<View>(R.id.recovery_words).visibility)
                screens.clearSecrets()
                assertTrue(retry.all { it == '\u0000' })
            } finally { retry.fill('\u0000') }
        } finally {
            display?.removeTextChangedListener(listener)
            delivery.close()
            words.fill('\u0000')
        }
    }
}
