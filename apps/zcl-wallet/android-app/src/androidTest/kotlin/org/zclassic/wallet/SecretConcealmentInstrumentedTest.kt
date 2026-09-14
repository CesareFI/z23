// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.text.Editable
import android.text.TextWatcher
import android.view.View
import android.widget.TextView
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import java.util.concurrent.atomic.AtomicReference
import org.junit.Assert.assertEquals
import org.junit.Assert.assertSame
import org.junit.Assert.assertThrows
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

/** Public markers only; no wallet, recovery phrase, key or authentication. */
@RunWith(AndroidJUnit4::class)
class SecretConcealmentInstrumentedTest {
    private val instrumentation = InstrumentationRegistry.getInstrumentation()

    private fun onMain(action: () -> Unit) {
        val failure = AtomicReference<Throwable?>()
        instrumentation.runOnMainSync {
            try { action() } catch (problem: Throwable) { failure.set(problem) }
        }
        failure.get()?.let { throw it }
    }

    private class Refusal : TextWatcher {
        var clearFailure: Throwable? = null
        var renderFailure: Throwable? = null
        private var rendered = false
        override fun beforeTextChanged(text: CharSequence?, start: Int, count: Int, after: Int) {
            if (after == 0 && (renderFailure == null || rendered)) clearFailure?.let { throw it }
        }
        override fun onTextChanged(text: CharSequence?, start: Int, before: Int, count: Int) = Unit
        override fun afterTextChanged(text: Editable?) {
            if (!text.isNullOrEmpty()) {
                rendered = true
                renderFailure?.let { throw it }
            }
        }
        fun allow() { clearFailure = null; renderFailure = null }
    }

    @Test fun failedDisplayClearConcealsTheFrameworkCopyAndWipesOwnedWords() {
        onMain {
            val view = RecoveryWordsView(instrumentation.targetContext)
            val refusal = Refusal()
            val words = charArrayOf('a', 'b', 'c')
            val problem = IllegalStateException("Public synthetic clear refusal")
            view.addTextChangedListener(refusal)
            try {
                view.show(words)
                refusal.clearFailure = problem
                assertSame(problem, assertThrows(IllegalStateException::class.java) { view.clearSecret() })
                assertTrue(words.all { it == '\u0000' })
                assertEquals("abc", view.text.toString()) // Real framework copy survives refused clear.
                assertEquals(View.INVISIBLE, view.visibility)
                refusal.allow()
                val retry = charArrayOf('z')
                try {
                    view.show(retry)
                    assertEquals(View.VISIBLE, view.visibility)
                    assertEquals("z", view.text.toString())
                    view.clearSecret()
                    assertTrue(retry.all { it == '\u0000' })
                    assertEquals(View.INVISIBLE, view.visibility)
                } finally { retry.fill('\u0000') }
            } finally { refusal.allow(); view.clearSecret(); words.fill('\u0000') }
        }
    }

    @Test fun failedDisplayRenderAndCleanupConcealThePartialFrameworkCopy() {
        onMain {
            val view = RecoveryWordsView(instrumentation.targetContext)
            val refusal = Refusal()
            val words = charArrayOf('a', 'b', 'c')
            val problem = OutOfMemoryError("Public synthetic post-render refusal")
            val cleanup = IllegalStateException("Public synthetic cleanup refusal")
            refusal.renderFailure = problem
            refusal.clearFailure = cleanup
            view.addTextChangedListener(refusal)
            try {
                assertSame(problem, assertThrows(OutOfMemoryError::class.java) { view.show(words) })
                assertEquals(listOf(cleanup), problem.suppressed.toList())
                assertTrue(words.all { it == '\u0000' })
                assertEquals("abc", view.text.toString())
                assertEquals(View.INVISIBLE, view.visibility)
            } finally { refusal.allow(); view.clearSecret(); words.fill('\u0000') }
        }
    }

    @Test fun failedKeyboardClearConcealsTheFrameworkCopy() = keyboardClearFailure(false)
    @Test fun failedKeyboardTransferConcealsTheFrameworkCopy() = keyboardClearFailure(true)

    private fun keyboardClearFailure(transfer: Boolean) {
        onMain {
            val view = RecoveryInputView(instrumentation.targetContext)
            val preview = view.getChildAt(0) as TextView
            val refusal = Refusal()
            val problem = IllegalStateException("Public synthetic keyboard clear refusal")
            preview.addTextChangedListener(refusal)
            try {
                for (letter in "abc") view.append(letter)
                refusal.clearFailure = problem
                assertSame(problem, assertThrows(IllegalStateException::class.java) {
                    if (transfer) view.takeInput().fill('\u0000') else view.clearSecret()
                })
                assertKeyboardCleared(view)
                assertEquals("abc", preview.text.toString())
                assertEquals(View.INVISIBLE, preview.visibility)
                refusal.allow()
                verifyKeyboardRetry(view, preview)
            } finally { refusal.allow(); view.clearSecret() }
        }
    }

    @Test fun failedKeyboardRenderAndCleanupConcealThePartialFrameworkCopy() {
        onMain {
            val view = RecoveryInputView(instrumentation.targetContext)
            val preview = view.getChildAt(0) as TextView
            val refusal = Refusal()
            val problem = OutOfMemoryError("Public synthetic keyboard post-render refusal")
            val cleanup = IllegalStateException("Public synthetic keyboard cleanup refusal")
            preview.addTextChangedListener(refusal)
            try {
                for (letter in "abc") view.append(letter)
                refusal.renderFailure = problem
                refusal.clearFailure = cleanup
                assertSame(problem, assertThrows(OutOfMemoryError::class.java) { view.append('d') })
                assertEquals(listOf(cleanup), problem.suppressed.toList())
                assertKeyboardCleared(view)
                assertEquals("abcd", preview.text.toString())
                assertEquals(View.INVISIBLE, preview.visibility)
                refusal.allow()
                verifyKeyboardRetry(view, preview)
            } finally { refusal.allow(); view.clearSecret() }
        }
    }

    private fun assertKeyboardCleared(view: RecoveryInputView) {
        val characters = RecoveryInputView::class.java.getDeclaredField("characters").apply {
            isAccessible = true
        }.get(view) as CharArray
        assertEquals(215, characters.size)
        assertTrue(characters.all { it == '\u0000' })
    }

    @Test fun ordinaryRenderingConcealsTextUntilEachUpdateCompletes() = onMain {
        val display = RecoveryWordsView(instrumentation.targetContext)
        val input = RecoveryInputView(instrumentation.targetContext)
        val preview = input.getChildAt(0) as TextView
        val words = charArrayOf('a', 'b', 'c')
        val observed = mutableListOf<Int>()
        fun observe(view: TextView) = object : TextWatcher {
            override fun beforeTextChanged(text: CharSequence?, start: Int, count: Int, after: Int) {
                observed.add(view.visibility)
            }
            override fun onTextChanged(text: CharSequence?, start: Int, before: Int, count: Int) = Unit
            override fun afterTextChanged(text: Editable?) { observed.add(view.visibility) }
        }
        display.addTextChangedListener(observe(display))
        preview.addTextChangedListener(observe(preview))
        try {
            display.show(words)
            assertEquals(View.VISIBLE, display.visibility)
            input.append('a')
            input.append('b')
            assertEquals(View.VISIBLE, preview.visibility)
            display.clearSecret()
            input.takeInput().fill('\u0000')
            assertTrue(observed.isNotEmpty())
            assertTrue("A text callback observed a visible incomplete update", observed.all { it == View.INVISIBLE })
        } finally { display.clearSecret(); input.clearSecret(); words.fill('\u0000') }
    }

    private fun verifyKeyboardRetry(view: RecoveryInputView, preview: TextView) {
        val empty = view.takeInput()
        try { assertEquals(0, empty.size) } finally { empty.fill('\u0000') }
        view.append('z')
        assertEquals(View.VISIBLE, view.visibility) // Keyboard remains usable.
        assertEquals(View.VISIBLE, preview.visibility)
        assertEquals("z", preview.text.toString())
        val retry = view.takeInput()
        try { assertEquals(listOf('z'), retry.toList()) } finally { retry.fill('\u0000') }
        assertEquals(View.INVISIBLE, preview.visibility)
        assertEquals(0, preview.text.length)
    }
}
