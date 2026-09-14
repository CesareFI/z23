// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.content.Context
import android.os.Parcelable
import android.text.Editable
import android.text.TextWatcher
import android.util.SparseArray
import android.widget.LinearLayout
import android.widget.TextView
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicReference
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertSame
import org.junit.Assert.assertThrows
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class SecretViewsInstrumentedTest {
    private fun oldTextState(context: Context, identifier: Int): SparseArray<Parcelable> {
        val saved = SparseArray<Parcelable>()
        TextView(context).apply {
            id = identifier
            freezesText = true
            text = "public saved recovery marker"
        }.saveHierarchyState(saved)
        assertTrue(saved.size() > 0)
        return saved
    }

    @Test fun oldFrameworkStateCannotPopulateAFreshRecoveryDisplay() {
        val instrumentation = InstrumentationRegistry.getInstrumentation()
        instrumentation.runOnMainSync {
            val context = instrumentation.targetContext
            val saved = oldTextState(context, R.id.recovery_words)
            val view = RecoveryWordsView(context).apply { id = R.id.recovery_words }
            try {
                view.restoreHierarchyState(saved)
                assertEquals(0, view.text.length)
                val outgoing = SparseArray<Parcelable>()
                view.saveHierarchyState(outgoing)
                assertEquals(0, outgoing.size())
            } finally { view.clearSecret() }
        }
    }

    @Test fun frameworkRestoreClearsExistingOwnedRecoveryWords() {
        val instrumentation = InstrumentationRegistry.getInstrumentation()
        instrumentation.runOnMainSync {
            val context = instrumentation.targetContext
            val saved = oldTextState(context, R.id.recovery_words)
            val words = charArrayOf('a', 'b', 'c')
            val view = RecoveryWordsView(context).apply { id = R.id.recovery_words }
            try {
                view.show(words)
                view.restoreHierarchyState(saved)
                assertTrue(words.all { it == '\u0000' })
                assertEquals(0, view.text.length)
            } finally { view.clearSecret(); words.fill('\u0000') }
        }
    }

    @Test fun keyboardRestoreDiscardsForeignFrameworkStateAndCurrentInput() {
        val instrumentation = InstrumentationRegistry.getInstrumentation()
        instrumentation.runOnMainSync {
            val context = instrumentation.targetContext
            val saved = oldTextState(context, R.id.recovery_input)
            val view = RecoveryInputView(context).apply { id = R.id.recovery_input }
            try {
                for (letter in "abc") view.append(letter)
                val outgoing = SparseArray<Parcelable>()
                view.saveHierarchyState(outgoing)
                assertEquals(0, outgoing.size())
                view.restoreHierarchyState(saved)
                val input = view.takeInput()
                try { assertEquals(0, input.size) } finally { input.fill('\u0000') }
                assertEquals(0, (view.getChildAt(0) as TextView).text.length)
            } finally { view.clearSecret() }
        }
    }

    private class RenderFailure(private vararg val problems: Throwable) : TextWatcher {
        private var calls = 0
        override fun beforeTextChanged(text: CharSequence?, start: Int, count: Int, after: Int) {
            if (calls < problems.size) throw problems[calls++]
        }
        override fun onTextChanged(text: CharSequence?, start: Int, before: Int, count: Int) = Unit
        override fun afterTextChanged(text: Editable?) = Unit
    }

    @Test fun appendRenderingFailureClearsInputAndPreservesTheOriginalFailure() = verifyRenderFailure(false)

    @Test fun failedPreviousDisplayClearStillClearsTheIncomingOwnedWords() {
        val instrumentation = InstrumentationRegistry.getInstrumentation()
        instrumentation.runOnMainSync {
            val view = RecoveryWordsView(instrumentation.targetContext)
            val previous = charArrayOf('a', 'b', 'c')
            val incoming = charArrayOf('d', 'e', 'f')
            val problem = IllegalStateException("Synthetic public old-display clear failure")
            val listener = RenderFailure(problem)
            try {
                view.show(previous)
                view.addTextChangedListener(listener)
                assertSame(problem, assertThrows(IllegalStateException::class.java) { view.show(incoming) })
                assertTrue(previous.all { it == '\u0000' })
                assertTrue("Failed replacement retained incoming words", incoming.all { it == '\u0000' })
                assertEquals(0, view.text.length)
            } finally {
                view.removeTextChangedListener(listener)
                view.clearSecret()
                previous.fill('\u0000')
                incoming.fill('\u0000')
            }
        }
    }

    @Test fun failedDisplayRenderingPreservesItsOriginalAndCleanupFailures() {
        val instrumentation = InstrumentationRegistry.getInstrumentation()
        instrumentation.runOnMainSync {
            val view = RecoveryWordsView(instrumentation.targetContext)
            val words = charArrayOf('a', 'b', 'c')
            val problem = OutOfMemoryError("Synthetic public new-display failure")
            val cleanup = IllegalArgumentException("Synthetic public display-cleanup failure")
            val listener = object : TextWatcher {
                private var started = false
                override fun beforeTextChanged(text: CharSequence?, start: Int, count: Int, after: Int) {
                    if (after > 0) { started = true; throw problem }
                    if (started) throw cleanup
                }
                override fun onTextChanged(text: CharSequence?, start: Int, before: Int, count: Int) = Unit
                override fun afterTextChanged(text: Editable?) = Unit
            }
            try {
                view.addTextChangedListener(listener)
                assertSame(problem, assertThrows(OutOfMemoryError::class.java) { view.show(words) })
                assertEquals(listOf(cleanup), problem.suppressed.toList())
                assertTrue(words.all { it == '\u0000' })
                view.removeTextChangedListener(listener)
                val retry = charArrayOf('z')
                try {
                    view.show(retry)
                    assertEquals(1, view.text.length)
                    view.clearSecret()
                    assertTrue(retry.all { it == '\u0000' })
                } finally { retry.fill('\u0000') }
            } finally {
                view.removeTextChangedListener(listener)
                view.clearSecret()
                words.fill('\u0000')
            }
        }
    }

    @Test fun deleteRenderingFailureClearsInputAndPreservesTheOriginalFailure() = verifyRenderFailure(true)

    private fun verifyRenderFailure(deleting: Boolean) {
        val instrumentation = InstrumentationRegistry.getInstrumentation()
        instrumentation.runOnMainSync {
            val view = RecoveryInputView(instrumentation.targetContext)
            val preview = view.getChildAt(0) as TextView
            val problem = OutOfMemoryError("Synthetic public rendering failure")
            val listener = RenderFailure(problem)
            try {
                for (letter in "abc") view.append(letter)
                preview.addTextChangedListener(listener)
                assertSame(problem, assertThrows(OutOfMemoryError::class.java) {
                    if (deleting) {
                        val controls = view.getChildAt(view.childCount - 1) as LinearLayout
                        assertTrue(controls.getChildAt(1).performClick())
                    } else view.append('d')
                })
                preview.removeTextChangedListener(listener)
                val input = view.takeInput()
                try { assertEquals(0, input.size) } finally { input.fill('\u0000') }
                assertEquals(0, preview.text.length)
            } finally {
                preview.removeTextChangedListener(listener)
                view.clearSecret()
            }
        }
    }

    @Test fun failedPreviewCleanupStillClearsKeyboardAndPreservesBothFailures() {
        val instrumentation = InstrumentationRegistry.getInstrumentation()
        instrumentation.runOnMainSync {
            val view = RecoveryInputView(instrumentation.targetContext)
            val preview = view.getChildAt(0) as TextView
            val problem = IllegalStateException("Synthetic public rendering failure")
            val cleanup = IllegalArgumentException("Synthetic public clearing failure")
            val listener = RenderFailure(problem, cleanup)
            try {
                for (letter in "abc") view.append(letter)
                preview.addTextChangedListener(listener)
                assertSame(problem, assertThrows(IllegalStateException::class.java) { view.append('d') })
                assertEquals(listOf(cleanup), problem.suppressed.toList())
                preview.removeTextChangedListener(listener)
                val input = view.takeInput()
                try { assertEquals(0, input.size) } finally { input.fill('\u0000') }
            } finally {
                preview.removeTextChangedListener(listener)
                view.clearSecret()
            }
        }
    }

    @Test fun failedInputTransferClearsItsSourceAndLeavesTheKeyboardUsable() {
        val instrumentation = InstrumentationRegistry.getInstrumentation()
        instrumentation.runOnMainSync {
            val view = RecoveryInputView(instrumentation.targetContext)
            val preview = view.getChildAt(0) as TextView
            val problem = IllegalStateException("Synthetic public preview-clear failure")
            val listener = RenderFailure(problem)
            try {
                for (letter in "abc") view.append(letter)
                preview.addTextChangedListener(listener)
                assertSame(problem, assertThrows(IllegalStateException::class.java) { view.takeInput().fill('\u0000') })
                preview.removeTextChangedListener(listener)
                val empty = view.takeInput()
                try { assertEquals(0, empty.size) } finally { empty.fill('\u0000') }
                view.append('z')
                val retry = view.takeInput()
                try { assertEquals(listOf('z'), retry.toList()) } finally { retry.fill('\u0000') }
            } finally {
                preview.removeTextChangedListener(listener)
                view.clearSecret()
            }
        }
    }

    @Test fun closingDeliveryClearsWordsBeforeTheMainQueueDrains() {
        val instrumentation = InstrumentationRegistry.getInstrumentation()
        val delivered = AtomicBoolean(false)
        val failure = AtomicReference<Throwable>()
        instrumentation.runOnMainSync {
            val owner = RecoveryPhraseDelivery(instrumentation.targetContext.mainExecutor)
            val words = charArrayOf('a', 'b', 'c') // Public markers; no wallet or key.
            val worker = Thread {
                try { owner.post(words) { delivered.set(true) } }
                catch (problem: Throwable) { failure.set(problem) }
            }
            try {
                worker.start()
                // Hold this main-thread fixture callback until the worker posts,
                // so the delivery cannot run before the cancellation assertion.
                worker.join(5000)
                assertFalse(worker.isAlive)
                assertEquals(null, failure.get())
                assertFalse(delivered.get())
                owner.close()
                assertTrue(words.all { it == '\u0000' })
            } finally {
                owner.close()
            }
        }
        instrumentation.waitForIdleSync()
        assertFalse(delivered.get())
    }

    @Test fun recoveryDisplayClearsOwnedArraysIncludingRejectedInput() {
        val instrumentation = InstrumentationRegistry.getInstrumentation()
        instrumentation.runOnMainSync {
            val view = RecoveryWordsView(instrumentation.targetContext)
            val first = charArrayOf('a', 'b', 'c') // Public test markers only.
            val second = charArrayOf('d', 'e', 'f')
            view.show(first)
            view.show(second)
            assertTrue(first.all { it == '\u0000' })
            view.clearSecret()
            assertTrue(second.all { it == '\u0000' })
            assertEquals(0, view.text.length)
            assertFalse(view.isSaveEnabled)
            val excessive = CharArray(216) { 'a' }
            assertThrows(IllegalArgumentException::class.java) { view.show(excessive) }
            assertTrue(excessive.all { it == '\u0000' })
        }
    }

    @Test fun recoveryKeyboardBoundsAndTransfersInputWithoutIme() {
        val instrumentation = InstrumentationRegistry.getInstrumentation()
        instrumentation.runOnMainSync {
            val view = RecoveryInputView(instrumentation.targetContext)
            view.append('A')
            view.append('\u202e')
            assertEquals(0, view.takeInput().size)
            repeat(1000) { view.append('a') }
            val input = view.takeInput()
            try {
                assertEquals(215, input.size)
                assertTrue(input.all { it == 'a' })
                assertEquals(0, view.takeInput().size)
                assertFalse(view.onCheckIsTextEditor())
                assertFalse(view.isSaveEnabled)
            } finally {
                input.fill('\u0000')
                view.clearSecret()
            }
        }
    }
}
