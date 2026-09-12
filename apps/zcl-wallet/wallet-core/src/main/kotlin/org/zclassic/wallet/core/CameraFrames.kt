// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet.core

import java.nio.ByteBuffer

/** Thin platform adapter; C validates, samples, packs and decodes. */
object CameraFrames {
    const val MAX_PACKET_BYTES = 147_461

    /** Borrow the plane only for this synchronous call. The Image must remain
     * open throughout. Caller owns and clears the returned packet. */
    fun pack(plane: ByteBuffer, width: Int, height: Int, rowStride: Int, pixelStride: Int): ByteArray? =
        NativeCore.packCameraPlane(plane, plane.position(), plane.remaining(), width, height, rowStride, pixelStride)

    /** Run in the isolated decoder service. Returned text is public request
     * data, and must be parsed again by the receiving process. Clear both arrays. */
    fun decode(packet: ByteArray, network: Network): ByteArray? =
        NativeCore.scanCameraPacket(packet, network.nativeId)
}
