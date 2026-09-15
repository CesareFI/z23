// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet.core

import kotlin.test.Test
import kotlin.test.assertFailsWith
import kotlin.test.assertSame
import kotlin.test.assertTrue

internal object SecretOutputNativeFixture {
    init { System.loadLibrary("zclwallet_secret_fixture") }
    @JvmStatic external fun phrase(input: ByteArray, output: CharArray, failure: Throwable, prefix: Int): Int
    @JvmStatic external fun entropy(input: CharArray, output: ByteArray, failure: Throwable, prefix: Int): Int
    @JvmStatic external fun camera(plane: java.nio.ByteBuffer, output: ByteArray, failure: Throwable, prefix: Int): Int
}

/** Public synthetic input, real VM arrays, actual production C JNI entries. */
class SecretOutputNativeTest {
    @Test fun realVmPhraseCopyFailurePreservesExceptionAndErasesOwnedArray() {
        val entropy = ByteArray(16) { it.toByte() }
        val expected = WalletKeys.recoveryPhrase(entropy)
        try {
            for (prefix in listOf(0, 1, expected.size / 2, expected.size, 215)) {
                val failure = OutOfMemoryError("public injected VM fixture")
                var retained: CharArray? = null
                val thrown = assertFailsWith<OutOfMemoryError> {
                    SecretOutput.characters(215) { output ->
                        retained = output
                        try {
                            SecretOutputNativeFixture.phrase(entropy, output, failure, prefix)
                        } catch (caught: OutOfMemoryError) {
                            assertSame(failure, caught)
                            val copied = minOf(prefix, expected.size)
                            assertTrue((0 until copied).all { output[it] == expected[it] })
                            assertTrue((copied until output.size).all { output[it] == '\u0000' })
                            throw caught
                        }
                    }
                }
                assertSame(failure, thrown)
                assertTrue(retained!!.all { it == '\u0000' })
            }
        } finally { entropy.fill(0); expected.fill('\u0000') }
    }

    @Test fun realVmEntropyCopyFailurePreservesExceptionAndErasesOwnedArray() {
        val expected = ByteArray(16) { (it + 1).toByte() }
        val phrase = WalletKeys.recoveryPhrase(expected)
        try {
            for (prefix in listOf(0, 1, 8, 16, 32)) {
                val failure = OutOfMemoryError("public injected VM fixture")
                var retained: ByteArray? = null
                val thrown = assertFailsWith<OutOfMemoryError> {
                    SecretOutput.bytes(32) { output ->
                        retained = output
                        try {
                            SecretOutputNativeFixture.entropy(phrase, output, failure, prefix)
                        } catch (caught: OutOfMemoryError) {
                            assertSame(failure, caught)
                            val copied = minOf(prefix, expected.size)
                            assertTrue((0 until copied).all { output[it] == expected[it] })
                            assertTrue((copied until output.size).all { output[it] == 0.toByte() })
                            throw caught
                        }
                    }
                }
                assertSame(failure, thrown)
                assertTrue(retained!!.all { it == 0.toByte() })
            }
        } finally { expected.fill(0); phrase.fill('\u0000') }
    }
}
