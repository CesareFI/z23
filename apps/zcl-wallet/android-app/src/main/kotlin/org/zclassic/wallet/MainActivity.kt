// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.app.Activity
import android.content.Intent
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.MotionEvent
import android.view.WindowManager
import java.io.File
import org.zclassic.wallet.core.CoreStatus
import org.zclassic.wallet.core.Network
import org.zclassic.wallet.core.TransparentAddress
import org.zclassic.wallet.core.WalletStorage

class MainActivity : Activity() {
    private lateinit var screens: WalletScreens
    private lateinit var authentication: WalletAuthentication
    private lateinit var storage: WalletStorage
    private val handler = Handler(Looper.getMainLooper())
    private var session: WalletPlatformSession? = null
    private var resumed = false
    private var busy = true
    private var setupTimeout: Runnable? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        window.addFlags(WindowManager.LayoutParams.FLAG_SECURE)
        if (Build.VERSION.SDK_INT >= 31) window.setHideOverlayWindows(true)
        screens = WalletScreens(this)
        storage = WalletStorage(File(noBackupFilesDir, "wallet-v1").absolutePath)
        authentication = WalletAuthentication(this, ::authenticated) {
            showFailure(R.string.authentication_failed)
        }
        screens.waiting()
    }

    override fun onResume() {
        super.onResume()
        resumed = true
        session = WalletPlatformSession(applicationContext, storage, mainExecutor)
        val pending = authentication.hasPending
        authentication.onResume()
        if (!pending) inspect()
    }

    override fun onPause() {
        resumed = false
        authentication.onPause()
        val previous = session
        session = null
        busy = true
        try { previous?.close() }
        finally {
            try {
                clearSetupTimeout()
                screens.waiting(R.string.wallet_backgrounded)
            } finally { super.onPause() }
        }
    }

    override fun onDestroy() {
        authentication.cancel()
        session?.close()
        session = null
        screens.clearSecrets()
        clearSetupTimeout()
        super.onDestroy()
    }

    override fun dispatchTouchEvent(event: MotionEvent): Boolean {
        val obscured = MotionEvent.FLAG_WINDOW_IS_OBSCURED or MotionEvent.FLAG_WINDOW_IS_PARTIALLY_OBSCURED
        if (event.flags and obscured != 0) return false
        return super.dispatchTouchEvent(event)
    }

    private fun inspect() {
        busy = true
        screens.waiting()
        session?.inspect({ stored ->
            busy = false
            when (stored.status) {
                CoreStatus.NOT_FOUND -> screens.welcome(
                    { prepare(WalletAction.CREATE, it) }, { prepare(WalletAction.RESTORE, it) }, ::openScanner)
                CoreStatus.OK -> showLocked(stored)
                else -> showFailure(R.string.storage_failed)
            }
        }, ::failed)
    }

    private fun showLocked(stored: WalletStorage.ReadResult) {
        val encoded = stored.record
        if (encoded == null) {
            showFailure(R.string.storage_failed)
            return
        }
        screens.locked(stored.pending) {
            operate { it.prepareUnlock(encoded, ::requestAuthentication, ::failed) }
        }
    }

    private fun operate(action: (WalletPlatformSession) -> Unit) {
        val active = session ?: return
        if (!resumed || busy) return
        busy = true
        screens.waiting()
        action(active)
    }

    private fun prepare(action: WalletAction, network: Network) = operate {
        it.prepare(action, network, ::requestAuthentication, ::failed)
    }

    private fun requestAuthentication(prepared: PreparedWalletAction) {
        screens.waiting(R.string.authentication_waiting)
        authentication.begin(prepared)
    }

    private fun authenticated(prepared: PreparedWalletAction) {
        val active = session ?: return
        busy = true
        screens.waiting()
        when (prepared.action) {
            WalletAction.CREATE -> active.createAfterAuthentication(prepared, { words ->
                if (!startSetupTimeout()) {
                    words.fill('\u0000')
                    return@createAfterAuthentication
                }
                busy = false
                screens.backup(words, { enterRecovery(confirming = true) }, ::restart)
            }, ::failed)
            WalletAction.RESTORE -> active.restoreAfterAuthentication(prepared, {
                if (startSetupTimeout()) enterRecovery(confirming = false)
            }, ::failed)
            WalletAction.UNLOCK -> active.unlockAfterAuthentication(prepared, ::received, ::failed)
        }
    }

    private fun enterRecovery(confirming: Boolean, retry: Boolean = false) {
        if (!resumed) return
        busy = false
        screens.enterRecovery(confirming, retry, { owned ->
            val active = session
            if (!resumed || busy || active == null) {
                owned.fill('\u0000')
            } else {
                busy = true
                screens.waiting()
                val invalid = { enterRecovery(confirming, retry = true) }
                if (confirming) active.confirmCreation(owned, invalid, ::received, ::failed)
                else active.restore(owned, invalid, ::received, ::failed)
            }
        }, ::restart)
    }

    private fun received(address: TransparentAddress) {
        clearSetupTimeout()
        busy = false
        screens.receive(address, ::restart) { openScanner(address.network) }
    }

    private fun openScanner(network: Network) {
        if (!resumed || busy) return
        busy = true
        try {
            startActivity(Intent(this, CameraScanActivity::class.java)
                .putExtra("mainnet", network == Network.MAINNET))
        } catch (_: Exception) {
            busy = false
            showFailure(R.string.operation_failed)
        }
    }

    private fun failed(problem: WalletProblem) = showFailure(when (problem) {
        WalletProblem.PROTECTION -> R.string.protection_failed
        WalletProblem.STORAGE -> R.string.storage_failed
        WalletProblem.OPERATION -> R.string.operation_failed
        WalletProblem.RESOURCES -> R.string.wallet_busy
    })

    private fun showFailure(message: Int) {
        clearSetupTimeout()
        session?.close()
        session = null
        busy = false
        screens.failure(message, ::restart)
    }

    private fun restart() {
        if (!resumed) return
        authentication.cancel()
        clearSetupTimeout()
        session?.close()
        session = WalletPlatformSession(applicationContext, storage, mainExecutor)
        inspect()
    }

    private fun startSetupTimeout(): Boolean {
        clearSetupTimeout()
        val timeout = Runnable { showFailure(R.string.setup_expired) }
        setupTimeout = timeout
        if (handler.postDelayed(timeout, 600_000)) return true
        showFailure(R.string.operation_failed)
        return false
    }

    private fun clearSetupTimeout() {
        setupTimeout?.let(handler::removeCallbacks)
        setupTimeout = null
    }
}
