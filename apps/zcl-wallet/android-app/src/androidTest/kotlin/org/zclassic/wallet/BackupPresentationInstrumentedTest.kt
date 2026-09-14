// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.app.Activity
import android.text.Editable
import android.text.TextWatcher
import android.view.View
import android.view.ViewGroup
import android.widget.LinearLayout
import android.widget.ScrollView
import androidx.test.core.app.ActivityScenario
import androidx.test.ext.junit.runners.AndroidJUnit4
import java.util.concurrent.atomic.AtomicReference
import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith

/** Public three-character markers in the debug display host; no wallet or key. */
@RunWith(AndroidJUnit4::class)
class BackupPresentationInstrumentedTest {
    private fun onActivity(scenario: ActivityScenario<WalletDisplayFixtureActivity>, action: (Activity) -> Unit) {
        val failure = AtomicReference<Throwable?>()
        scenario.onActivity { activity ->
            try { action(activity) } catch (problem: Throwable) { failure.set(problem) }
        }
        failure.get()?.let { throw it }
    }

    private fun withScreens(action: (Activity, WalletScreens, LinearLayout) -> Unit) {
        ActivityScenario.launch(WalletDisplayFixtureActivity::class.java).use { scenario ->
            onActivity(scenario) { activity ->
                val screens = WalletScreens(activity)
                val root = activity.findViewById<ViewGroup>(android.R.id.content)
                val content = (root.getChildAt(0) as ScrollView).getChildAt(0) as LinearLayout
                try { action(activity, screens, content) }
                finally {
                    content.setOnHierarchyChangeListener(null)
                    screens.clearSecrets()
                }
            }
        }
    }

    @Test fun failedBackupConfirmationControlDoesNotPublishWords() = controlFailure(R.id.backup_written)
    @Test fun failedCancelControlDoesNotPublishWords() = controlFailure(R.id.cancel_setup)

    @Test fun recreationClearsPublishedWordsAndDoesNotRestoreTheBackup() {
        val words = charArrayOf('a', 'b', 'c')
        try {
            ActivityScenario.launch(WalletDisplayFixtureActivity::class.java).use { scenario ->
                var previous: Activity? = null
                onActivity(scenario) { activity ->
                    previous = activity
                    WalletScreens(activity).backup(words, {}, {})
                    assertEquals("abc", activity.findViewById<RecoveryWordsView>(R.id.recovery_words).text.toString())
                }
                scenario.recreate()
                assertTrue("Recreation retained the previous words", words.all { it == '\u0000' })
                onActivity(scenario) { activity ->
                    assertNotSame(previous, activity)
                    assertNull(activity.findViewById<View>(R.id.recovery_words))
                    assertNull(activity.findViewById<View>(R.id.backup_written))
                }
            }
        } finally { words.fill('\u0000') }
    }

    @Test fun failedPreviousScreenClearAlsoWipesIncomingWords() = withScreens { activity, screens, _ ->
        val previous = charArrayOf('a', 'b', 'c')
        val incoming = charArrayOf('d', 'e', 'f')
        screens.backup(previous, {}, {})
        val display = activity.findViewById<RecoveryWordsView>(R.id.recovery_words)
        val problem = IllegalStateException("Synthetic public previous-screen cleanup failure")
        val listener = object : TextWatcher {
            override fun beforeTextChanged(text: CharSequence?, start: Int, count: Int, after: Int) {
                if (after == 0) throw problem
            }
            override fun onTextChanged(text: CharSequence?, start: Int, before: Int, count: Int) = Unit
            override fun afterTextChanged(text: Editable?) = Unit
        }
        display.addTextChangedListener(listener)
        try {
            assertSame(problem, assertThrows(IllegalStateException::class.java) {
                screens.backup(incoming, {}, {})
            })
            assertTrue(previous.all { it == '\u0000' })
            assertTrue("Incoming words survived failed screen cleanup", incoming.all { it == '\u0000' })
            assertEquals(View.INVISIBLE, display.visibility)
        } finally {
            display.removeTextChangedListener(listener)
            previous.fill('\u0000')
            incoming.fill('\u0000')
        }
    }

    private fun controlFailure(failedControl: Int) = withScreens { activity, screens, content ->
        val words = charArrayOf('a', 'b', 'c')
        val problem = OutOfMemoryError("Synthetic public backup control failure")
        content.setOnHierarchyChangeListener(object : ViewGroup.OnHierarchyChangeListener {
            override fun onChildViewAdded(parent: View?, child: View?) {
                if (child?.id == failedControl) throw problem
            }
            override fun onChildViewRemoved(parent: View?, child: View?) = Unit
        })
        try {
            assertSame(problem, assertThrows(OutOfMemoryError::class.java) {
                screens.backup(words, {}, {})
            })
            val display = activity.findViewById<RecoveryWordsView>(R.id.recovery_words)
            assertTrue("Incomplete backup published a framework copy", display.text.isEmpty())
            assertTrue("Failed backup retained transferred characters", words.all { it == '\u0000' })
            content.setOnHierarchyChangeListener(null)
            val retry = charArrayOf('z')
            try {
                screens.backup(retry, {}, {})
                assertEquals("z", activity.findViewById<RecoveryWordsView>(R.id.recovery_words).text.toString())
                assertNotNull(activity.findViewById<View>(R.id.backup_written))
                assertNotNull(activity.findViewById<View>(R.id.cancel_setup))
                screens.clearSecrets()
                assertTrue(retry.all { it == '\u0000' })
            } finally { retry.fill('\u0000') }
        } finally { words.fill('\u0000') }
    }

    @Test fun successfulBackupPublishesWordsOnlyAfterBothControlsExist() = withScreens { activity, screens, content ->
        var observed = false
        content.setOnHierarchyChangeListener(object : ViewGroup.OnHierarchyChangeListener {
            override fun onChildViewAdded(parent: View?, child: View?) {
                if (child !is RecoveryWordsView) return
                child.addTextChangedListener(object : TextWatcher {
                    override fun beforeTextChanged(text: CharSequence?, start: Int, count: Int, after: Int) = Unit
                    override fun onTextChanged(text: CharSequence?, start: Int, before: Int, count: Int) = Unit
                    override fun afterTextChanged(text: Editable?) {
                        if (text.isNullOrEmpty()) return
                        assertNotNull(activity.findViewById<View>(R.id.backup_written))
                        assertNotNull(activity.findViewById<View>(R.id.cancel_setup))
                        observed = true
                    }
                })
            }
            override fun onChildViewRemoved(parent: View?, child: View?) = Unit
        })
        val words = charArrayOf('a', 'b', 'c')
        try {
            screens.backup(words, {}, {})
            assertTrue(observed)
            assertEquals(View.VISIBLE, activity.findViewById<RecoveryWordsView>(R.id.recovery_words).visibility)
        } finally { words.fill('\u0000') }
    }
}
