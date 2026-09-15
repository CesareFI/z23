// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet.core

import java.nio.ByteBuffer

/** Thin platform adapter; C validates, samples, packs and decodes. */
object CameraFrames {
    const val MAX_PACKET_BYTES = 147_461

    /** Borrow the plane only for this synchronous call. The Image must remain
     * open throughout. Caller owns and clears the returned packet. */
    fun pack(plane: ByteBuffer, width: Int, height: Int, rowStride: Int, pixelStride: Int): ByteArray? {
        val offset = plane.position()
        val length = plane.remaining()
        val capacity = NativeCore.cameraPlanePacketSize(plane, offset, length, width, height, rowStride, pixelStride)
        return ownedPacket(capacity) { output ->
            NativeCore.packCameraPlane(plane, offset, length, width, height, rowStride, pixelStride, output)
        }
    }

    /** Ownership only; C validates geometry and the exact output length again.
     * A partial VM transfer still leaves this caller able to erase its array. */
    internal inline fun ownedPacket(capacity: Int, write: (ByteArray) -> Int): ByteArray? {
        if (capacity !in 1..MAX_PACKET_BYTES) return null
        val output = ByteArray(capacity)
        var transferred = false
        try {
            if (write(output) != capacity) return null
            transferred = true
            return output
        } finally {
            if (!transferred) output.fill(0)
        }
    }

    /** Run in the isolated decoder service. Returned text is public request
     * data, and must be parsed again by the receiving process. Clear both arrays. */
    fun decode(packet: ByteArray, network: Network): ByteArray? =
        NativeCore.scanCameraPacket(packet, network.nativeId)
}
