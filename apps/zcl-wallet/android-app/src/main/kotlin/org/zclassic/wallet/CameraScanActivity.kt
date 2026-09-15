// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.Manifest
import android.app.Activity
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.view.MotionEvent
import android.view.WindowManager
import org.zclassic.wallet.core.Network
import org.zclassic.wallet.core.PaymentRequest

/** Private Activity: camera permission/lifecycle and public request presentation.
 * Scanning never opens a wallet session or authorizes a transfer. */
class CameraScanActivity : Activity() {
    private companion object { const val NETWORK_STATE = "scan_network_mainnet" }
    private lateinit var screens: ScanScreens
    private var network = Network.TESTNET
    private var resumed = false
    private var permissionPending = false
    private var startAfterPermission = false
    private var permissionDenied = false
    private var decoder: ScanDecodeClient? = null
    private var camera: CameraCapture? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        window.addFlags(WindowManager.LayoutParams.FLAG_SECURE)
        if (Build.VERSION.SDK_INT >= 31) window.setHideOverlayWindows(true)
        val launchMainnet = intent.getBooleanExtra("mainnet", false)
        network = if (savedInstanceState?.getBoolean(NETWORK_STATE, launchMainnet) ?: launchMainnet)
            Network.MAINNET else Network.TESTNET
        screens = ScanScreens(this)
    }

    override fun onSaveInstanceState(outState: Bundle) {
        // Public preference only. Capture, decoded requests and permission
        // continuation still require a new foreground action after recreation.
        outState.putBoolean(NETWORK_STATE, network == Network.MAINNET)
        super.onSaveInstanceState(outState)
    }

    override fun onResume() {
        super.onResume()
        resumed = true
        if (startAfterPermission) {
            startAfterPermission = false
            start(network)
        } else choose(if (permissionDenied) R.string.scan_permission_denied else R.string.scan_description)
    }

    override fun onPause() {
        resumed = false
        startAfterPermission = false
        try { stop() } finally { super.onPause() }
    }

    override fun onDestroy() {
        try { stop() } finally { super.onDestroy() }
    }

    override fun dispatchTouchEvent(event: MotionEvent): Boolean {
        if (event.flags and (MotionEvent.FLAG_WINDOW_IS_OBSCURED or MotionEvent.FLAG_WINDOW_IS_PARTIALLY_OBSCURED) != 0) return false
        return super.dispatchTouchEvent(event)
    }

    private fun choose(message: Int = R.string.scan_description) {
        stop()
        if (resumed) screens.choose(network, message, { network = it }, ::start, ::finish)
    }

    private fun start(selected: Network) {
        if (!resumed || permissionPending || decoder != null || camera != null) return
        network = selected
        permissionDenied = false
        if (checkSelfPermission(Manifest.permission.CAMERA) != PackageManager.PERMISSION_GRANTED) {
            permissionPending = true
            requestPermissions(arrayOf(Manifest.permission.CAMERA), 1)
            return
        }
        screens.scanning(network) { choose() }
        val client = ScanDecodeClient(this, ::decoderReady, onFailure = { owner ->
            if (resumed && decoder === owner) choose(R.string.scan_unavailable)
        })
        decoder = client
        client.connect()
    }

    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<out String>, grantResults: IntArray) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode != 1 || !permissionPending) return
        permissionPending = false
        val granted = grantResults.size == 1 && grantResults[0] == PackageManager.PERMISSION_GRANTED &&
            checkSelfPermission(Manifest.permission.CAMERA) == PackageManager.PERMISSION_GRANTED
        if (!granted) {
            // Android may deliver the result before onResume. Keep this public
            // explanation until a new attempt; it grants no capture continuation.
            permissionDenied = true
            if (resumed) choose(R.string.scan_permission_denied)
            return
        }
        if (resumed) start(network) else startAfterPermission = true
    }

    private fun decoderReady(owner: ScanDecodeClient) {
        if (!resumed || decoder !== owner) { owner.close(); return }
        val capture = CameraCapture(this, ::frame) { failed ->
            if (resumed && camera === failed) choose(R.string.scan_unavailable)
        }
        camera = capture
        capture.start()
    }

    private fun frame(owner: CameraCapture, packet: ByteArray, orientation: Int, front: Boolean) {
        if (!resumed || camera !== owner) { owner.frameDone(); return }
        try {
            screens.frame(packet, orientation, front)
            val client = decoder
            if (client == null || !client.submit(packet, network) { request -> decoded(owner, request) })
                choose(R.string.scan_unavailable)
        } catch (_: Exception) {
            choose(R.string.scan_unavailable)
        }
    }

    private fun decoded(owner: CameraCapture, request: PaymentRequest?) {
        owner.frameDone()
        if (!resumed || camera !== owner || request == null) return
        stop()
        screens.review(request, { start(network) }, ::finish)
    }

    private fun stop() {
        // Each owner must be retired even if another close fails. Keep failed
        // owners available for a later lifecycle retry and rethrow the first
        // failure without allocating suppressed-exception storage.
        var failure: Throwable? = null
        try { camera?.close(); camera = null }
        catch (problem: Throwable) { failure = problem }
        try { decoder?.close(); decoder = null }
        catch (problem: Throwable) { if (failure == null) failure = problem }
        try { screens.clear() }
        catch (problem: Throwable) { if (failure == null) failure = problem }
        if (failure != null) throw failure
    }
}
