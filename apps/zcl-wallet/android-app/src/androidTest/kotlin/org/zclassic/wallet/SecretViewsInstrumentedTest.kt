// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertThrows
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class SecretViewsInstrumentedTest {
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
