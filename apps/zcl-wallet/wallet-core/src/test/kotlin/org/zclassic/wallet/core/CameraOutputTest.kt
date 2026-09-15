// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet.core

import java.nio.ByteBuffer
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertSame
import kotlin.test.assertTrue

class CameraOutputTest {
    @Test fun refusedOutputLengthsEraseTheWholeOwnedPacket() {
        for (capacity in listOf(446, 76805, CameraFrames.MAX_PACKET_BYTES)) {
            for (written in listOf(Int.MIN_VALUE, -1, 0, capacity - 1, capacity + 1, Int.MAX_VALUE)) {
                var owned: ByteArray? = null
                assertNull(CameraFrames.ownedPacket(capacity) { output ->
                    owned = output
                    output.fill(93)
                    written
                })
                assertTrue(owned!!.all { it == 0.toByte() })
            }
        }
    }

    @Test fun allocationBoundsRefuseBeforeTheWriterRuns() {
        for (capacity in listOf(Int.MIN_VALUE, -1, 0, CameraFrames.MAX_PACKET_BYTES + 1, Int.MAX_VALUE)) {
            assertNull(CameraFrames.ownedPacket(capacity) { error("Writer reached for invalid capacity") })
        }
    }

    @Test fun completePacketTransfersItsExactAllocationWithoutCopying() {
        for (capacity in listOf(446, 76805, CameraFrames.MAX_PACKET_BYTES)) {
            var owned: ByteArray? = null
            val result = assertNotNull(CameraFrames.ownedPacket(capacity) { output ->
                owned = output
                output.fill(93)
                output.size
            })
            try {
                assertSame(owned, result)
                assertEquals(capacity, result.size)
                assertTrue(result.all { it == 93.toByte() })
            } finally { result.fill(0) }
        }
    }

    @Test fun realVmPartialPixelCopyPreservesExceptionAndErasesCallerOutput() {
        val pixels = ByteArray(441) { 93 }
        val plane = ByteBuffer.allocateDirect(pixels.size).apply { put(pixels); flip() }
        val expected = assertNotNull(CameraFrames.pack(plane, 21, 21, 21, 1))
        try {
            for (prefix in listOf(0, 1, 6, 223, 446)) {
                val failure = OutOfMemoryError("Public injected camera output failure")
                var retained: ByteArray? = null
                val thrown = assertFailsWith<OutOfMemoryError> {
                    CameraFrames.ownedPacket(expected.size) { output ->
                        retained = output
                        try { SecretOutputNativeFixture.camera(plane, output, failure, prefix) }
                        catch (caught: OutOfMemoryError) {
                            assertSame(failure, caught)
                            assertTrue((0 until prefix).all { output[it] == expected[it] })
                            assertTrue((prefix until output.size).all { output[it] == 0.toByte() })
                            throw caught
                        }
                    }
                }
                assertSame(failure, thrown)
                assertTrue(retained!!.all { it == 0.toByte() })
            }
            val after = ByteArray(pixels.size)
            plane.get(after)
            assertContentEquals(pixels, after)
            after.fill(0)
        } finally {
            pixels.fill(0)
            expected.fill(0)
            plane.clear()
            while (plane.hasRemaining()) plane.put(0)
        }
    }
}
