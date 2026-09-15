// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.app.Service
import android.content.Intent
import android.os.Binder
import android.os.IBinder
import java.util.concurrent.atomic.AtomicBoolean
import org.zclassic.wallet.core.CameraFrames
import org.zclassic.wallet.core.Network
import org.zclassic.wallet.scanipc.IScanDecoder
import org.zclassic.wallet.scanipc.IScanReply

/** Android IPC/lifetime adapter. Manifest isolation removes app permissions;
 * C owns all image, QR and receiving-request processing. One frame at a time. */
class ScanDecodeService : Service() {
    private val worker = OwnedExecutor()
    private val busy = AtomicBoolean()
    private val stopped = AtomicBoolean()

    private val endpoint = object : IScanDecoder.Stub() {
        override fun ready(reply: IScanReply?) {
            if (!allowed() || reply == null) return
            try { reply.onReady() } catch (_: Exception) { /* Caller disconnected. */ }
        }

        override fun decode(frame: ByteArray?, network: Int, requestId: Long, reply: IScanReply?) {
            if (!allowed()) { frame?.fill(0); return }
            if (frame == null || reply == null) { frame?.fill(0); return }
            val chain = when (network) { 0 -> Network.MAINNET; 1 -> Network.TESTNET; else -> null }
            if (chain == null || requestId <= 0 || frame.size > CameraFrames.MAX_PACKET_BYTES ||
                !busy.compareAndSet(false, true)) {
                frame.fill(0)
                respond(reply, requestId, null)
                return
            }
            val submitted = worker.submit(cleanup = { frame.fill(0); busy.set(false) }) {
                var text: ByteArray? = null
                try {
                    if (!stopped.get()) text = CameraFrames.decode(frame, chain)
                } catch (_: Exception) {
                    // Generic refusal; never log camera frames or payloads.
                } finally {
                    // Native decoding has returned. Retire captured pixels
                    // before Binder can allocate or block while sending text.
                    // Task cleanup still owns queued/rejected input retirement.
                    frame.fill(0)
                    try {
                        if (!stopped.get()) respond(reply, requestId, text)
                    } finally {
                        text?.fill(0)
                    }
                }
            }
            if (!submitted) respond(reply, requestId, null)
        }
    }

    private fun allowed(): Boolean = !stopped.get() && Binder.getCallingUid() == applicationInfo.uid

    private fun respond(reply: IScanReply, requestId: Long, text: ByteArray?) {
        try { reply.onResult(requestId, text) } catch (_: Exception) { /* No retry queue. */ }
    }

    override fun onBind(intent: Intent?): IBinder = endpoint

    override fun onDestroy() {
        stopped.set(true)
        worker.close()
        super.onDestroy()
    }
}
