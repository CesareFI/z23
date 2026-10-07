// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet.core

import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertSame
import kotlin.test.assertTrue

class SecretOutputTest {
    @Test fun partialNativeCopyAndExceptionRetainAnOwnerForErasure() {
        val failure = IllegalStateException("public fixture failure")
        var bytes: ByteArray? = null
        var chars: CharArray? = null
        assertSame(failure, assertFailsWith<IllegalStateException> {
            SecretOutput.bytes(32) { output ->
                bytes = output
                output.fill(0x42, 0, 17)
                throw failure
            }
        })
        assertTrue(bytes!!.all { it == 0.toByte() })
        assertSame(failure, assertFailsWith<IllegalStateException> {
            SecretOutput.characters(215) { output ->
                chars = output
                output.fill('a', 0, 107)
                throw failure
            }
        })
        assertTrue(chars!!.all { it == '\u0000' })
    }

    @Test fun everyRefusedLengthErasesTheCompleteDestination() {
        for (length in listOf(Int.MIN_VALUE, -1, 0, 33, Int.MAX_VALUE)) {
            var owned: ByteArray? = null
            assertFailsWith<IllegalStateException> {
                SecretOutput.bytes(32) { output -> owned = output; output.fill(0x42); length }
            }
            assertTrue(owned!!.all { it == 0.toByte() })
        }
        for (length in listOf(Int.MIN_VALUE, -1, 0, 216, Int.MAX_VALUE)) {
            var owned: CharArray? = null
            assertFailsWith<IllegalStateException> {
                SecretOutput.characters(215) { output -> owned = output; output.fill('a'); length }
            }
            assertTrue(owned!!.all { it == '\u0000' })
        }
    }

    @Test fun twelveWordCreationRetiresEveryPartialOutputOnFailure() {
        for (prefix in 0..16) {
            var bytes: ByteArray? = null
            assertFailsWith<IllegalStateException> {
                SecretOutput.bytes(16) { output ->
                    bytes = output
                    output.fill(0x42, 0, prefix)
                    throw IllegalStateException("public fixture failure")
                }
            }
            assertTrue(bytes!!.all { it == 0.toByte() })
        }
    }

    @Test fun exactResultTransfersTheOneOwner() {
        var bytes: ByteArray? = null
        var chars: CharArray? = null
        val result = SecretOutput.bytes(32) { bytes = it; it.fill(0x42); it.size }
        val words = SecretOutput.characters(215) { chars = it; it.fill('a'); it.size }
        try {
            assertSame(bytes, result)
            assertSame(chars, words)
            assertTrue(result.all { it == 0x42.toByte() })
            assertTrue(words.all { it == 'a' })
        } finally { result.fill(0); words.fill('\u0000') }
    }

    @Test fun shortResultErasesFullScratchAndReturnsOnlyTheWrittenPrefix() {
        for (length in 1 until 32) {
            var scratch: ByteArray? = null
            val result = SecretOutput.bytes(32) { scratch = it; it.fill(0x42); length }
            try {
                assertContentEquals(ByteArray(length) { 0x42 }, result)
                assertTrue(scratch!!.all { it == 0.toByte() })
            } finally { result.fill(0) }
        }
        for (length in 1 until 215) {
            var scratch: CharArray? = null
            val result = SecretOutput.characters(215) { scratch = it; it.fill('a'); length }
            try {
                assertContentEquals(CharArray(length) { 'a' }, result)
                assertTrue(scratch!!.all { it == '\u0000' })
            } finally { result.fill('\u0000') }
        }
    }

    @Test fun capacitiesAreBoundedBeforeTheWriterRuns() {
        for (capacity in listOf(Int.MIN_VALUE, -1, 0, 33, Int.MAX_VALUE)) {
            assertFailsWith<IllegalArgumentException> {
                SecretOutput.bytes(capacity) { error("writer reached") }
            }
        }
        for (capacity in listOf(Int.MIN_VALUE, -1, 0, 216, Int.MAX_VALUE)) {
            assertFailsWith<IllegalArgumentException> {
                SecretOutput.characters(capacity) { error("writer reached") }
            }
        }
    }
}
