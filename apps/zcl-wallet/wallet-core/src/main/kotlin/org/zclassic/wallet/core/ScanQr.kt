// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet.core

/** Public luminance frame adapter. C owns decoding, bounds and request validation.
 * Caller owns a stable array for the call and clears it afterwards. No logging,
 * persistence, native handle, secret import or payment authorization.
 */
object ScanQr {
    fun decode(image: ByteArray, width: Int, height: Int, rowStride: Int,
               pixelStride: Int, network: Network): PaymentRequest? {
        val record = NativeCore.scanQr(image, width, height, rowStride, pixelStride, network.nativeId)
            ?: return null
        return PaymentRequest.fromNative(record, network)
    }
}
