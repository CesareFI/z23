// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.ServiceConnection
import android.content.pm.PackageManager
import android.content.pm.ServiceInfo
import android.os.Binder
import android.os.Build
import android.os.Handler
import android.os.IBinder
import android.os.Looper
import android.os.Process
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicInteger
import java.util.concurrent.atomic.AtomicLong
import java.util.concurrent.atomic.AtomicReference
import org.zclassic.wallet.core.CameraFrames
import org.zclassic.wallet.core.Network
import org.zclassic.wallet.core.PaymentRequest
import org.zclassic.wallet.scanipc.IScanDecoder
import org.zclassic.wallet.scanipc.IScanReply

/** Bounded Android Binder adapter. It owns at most one outstanding request and
 * one reply array. C revalidates all returned text before any UI display. */
internal class ScanDecodeClient(
    context: Context,
    private val onReady: (ScanDecodeClient) -> Unit,
    private val onFailure: (ScanDecodeClient) -> Unit,
) : AutoCloseable {
    private val context = context.applicationContext
    private val main = Handler(Looper.getMainLooper())
    private val closed = AtomicBoolean()
    private val bound = AtomicBoolean()
    private val started = AtomicBoolean()
    private val service = AtomicReference<IScanDecoder?>()
    private val serviceUid = AtomicInteger(-1)
    private val sequence = AtomicLong()
    private val pending = AtomicReference<Pending?>()
    private val connectTimeout = Runnable { if (!isReady) fail() }
    val isReady: Boolean get() = !closed.get() && serviceUid.get() >= 0 && service.get() != null
    internal val decoderUid: Int get() = serviceUid.get() // Instrumentation observes Binder identity.

    private class Pending(val id: Long, val network: Network,
                          val deliver: (PaymentRequest?) -> Unit, expire: (Long) -> Unit) {
        val claimed = AtomicBoolean()
        val bytes = AtomicReference<ByteArray?>()
        val timeout = Runnable { expire(id) }
        fun clear() { bytes.getAndSet(null)?.fill(0) }
    }

    private val reply = object : IScanReply.Stub() {
        override fun onReady() {
            val uid = Binder.getCallingUid()
            if (closed.get()) return
            if (uid < 0 || uid == Process.myUid()) { fail(); return }
            if (!serviceUid.compareAndSet(-1, uid)) return
            main.removeCallbacks(connectTimeout)
            if (!main.post { if (isReady) onReady(this@ScanDecodeClient) }) fail()
        }

        override fun onResult(requestId: Long, text: ByteArray?) {
            val request = pending.get()
            if (closed.get() || Binder.getCallingUid() != serviceUid.get() ||
                request == null || request.id != requestId || !request.claimed.compareAndSet(false, true)) {
                text?.fill(0)
                return
            }
            if (text != null && text.size > 1024) { text.fill(0); fail(); return }
            request.bytes.set(text ?: ByteArray(0))
            if (closed.get() || pending.get() !== request) { request.clear(); return }
            if (!main.post { deliver(request) }) { request.clear(); fail() }
        }
    }

    private val connection = object : ServiceConnection {
        override fun onServiceConnected(name: ComponentName, binder: IBinder) {
            if (closed.get()) return
            if (name != component()) { fail(); return }
            val endpoint = IScanDecoder.Stub.asInterface(binder)
            service.set(endpoint)
            try { endpoint.ready(reply) } catch (_: Exception) { fail() }
        }
        override fun onServiceDisconnected(name: ComponentName) = fail()
        override fun onBindingDied(name: ComponentName) = fail()
        override fun onNullBinding(name: ComponentName) = fail()
    }

    private fun component() = ComponentName(context, ScanDecodeService::class.java)

    @Suppress("DEPRECATION") // API 30..32 use the integer flags overload.
    private fun isolatedComponent(): Boolean {
        val info = if (Build.VERSION.SDK_INT >= 33)
            context.packageManager.getServiceInfo(component(), PackageManager.ComponentInfoFlags.of(0))
        else context.packageManager.getServiceInfo(component(), 0)
        return !info.exported && info.flags and ServiceInfo.FLAG_ISOLATED_PROCESS != 0
    }

    fun connect() {
        if (closed.get() || !started.compareAndSet(false, true)) return
        try {
            if (!isolatedComponent() || !main.postDelayed(connectTimeout, 15_000)) { fail(); return }
            val accepted = context.bindService(Intent().setComponent(component()), connection, Context.BIND_AUTO_CREATE)
            if (!accepted) { fail(); return }
            bound.set(true)
            if (closed.get()) unbind()
        } catch (_: Exception) { fail() }
    }

    /** Consumes and clears frame on every path, including a busy/refused call. */
    fun submit(frame: ByteArray, network: Network, deliver: (PaymentRequest?) -> Unit): Boolean {
        try {
            val endpoint = service.get() ?: return false
            if (!isReady || frame.size !in 5..CameraFrames.MAX_PACKET_BYTES) return false
            val id = sequence.incrementAndGet()
            if (id <= 0) { fail(); return false }
            val request = Pending(id, network, deliver) { expired ->
                if (pending.get()?.id == expired) fail()
            }
            if (!pending.compareAndSet(null, request)) return false
            if (!main.postDelayed(request.timeout, 5_000)) { fail(); return false }
            if (closed.get()) { clearPending(); return false }
            endpoint.decode(frame, if (network == Network.MAINNET) 0 else 1, id, reply)
            return true
        } catch (_: Exception) {
            fail()
            return false
        } finally {
            frame.fill(0)
        }
    }

    private fun deliver(request: Pending) {
        if (!pending.compareAndSet(request, null)) { request.clear(); return }
        main.removeCallbacks(request.timeout)
        val bytes = request.bytes.getAndSet(null) ?: return
        try {
            if (closed.get()) return
            val parsed = if (bytes.isEmpty()) null else PaymentRequest.parseEncoded(bytes, request.network)
            if (bytes.isNotEmpty() && parsed == null) { fail(); return }
            if (!closed.get()) request.deliver(parsed)
        } catch (_: Exception) {
            fail()
        } finally {
            bytes.fill(0)
        }
    }

    private fun clearPending() {
        pending.getAndSet(null)?.let {
            main.removeCallbacks(it.timeout)
            it.clear()
        }
    }

    private fun unbind() {
        if (bound.compareAndSet(true, false)) {
            try { context.unbindService(connection) } catch (_: IllegalArgumentException) { /* Already detached. */ }
        }
    }

    private fun fail() {
        if (!closed.compareAndSet(false, true)) return
        close()
        // The Activity also checks its own lifetime before changing its views.
        if (!main.post { onFailure(this) }) close()
    }

    override fun close() {
        closed.set(true)
        main.removeCallbacks(connectTimeout)
        clearPending()
        service.set(null)
        unbind()
    }
}
