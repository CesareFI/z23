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
    private var setupWindow: SetupWindow? = null

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
        resumed = false
        busy = true
        val previous = session
        session = null
        // Match scanner teardown: attempt every owner, preserve the first
        // failure without allocating suppressed-exception storage, and always
        // reach framework destruction. Creation may have installed only some
        // owners. An active worker still owns cleanup until its termination.
        var failure: Throwable? = null
        try {
            try { if (this::authentication.isInitialized) authentication.cancel() }
            catch (problem: Throwable) { failure = problem }
            try { previous?.close() }
            catch (problem: Throwable) { if (failure == null) failure = problem }
            try { clearSetupTimeout() }
            catch (problem: Throwable) { if (failure == null) failure = problem }
            try { if (this::screens.isInitialized) screens.clearSecrets() }
            catch (problem: Throwable) { if (failure == null) failure = problem }
            if (failure != null) throw failure
        } finally { super.onDestroy() }
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
            WalletAction.CREATE -> {
                val window = SetupWindow()
                active.createAfterAuthentication(prepared, window, { words ->
                    if (!startSetupTimeout(window)) {
                        words.fill('\u0000')
                        return@createAfterAuthentication
                    }
                    busy = false
                    screens.backup(words, { enterRecovery(confirming = true) }, ::restart)
                }, ::failed)
            }
            WalletAction.RESTORE -> {
                val window = SetupWindow()
                active.restoreAfterAuthentication(prepared, window, {
                    if (startSetupTimeout(window)) enterRecovery(confirming = false)
                }, ::failed)
            }
            WalletAction.UNLOCK -> active.unlockAfterAuthentication(prepared, ::received, ::failed)
        }
    }

    private fun enterRecovery(confirming: Boolean, retry: Boolean = false) {
        if (!resumed || !setupOpen()) return
        busy = false
        screens.enterRecovery(confirming, retry, { owned -> submitRecovery(owned, confirming) }, ::restart)
    }

    private fun submitRecovery(owned: CharArray, confirming: Boolean) {
        var transferred = false
        try {
            val active = session
            if (!resumed || busy || active == null || !setupOpen()) return
            busy = true
            screens.waiting()
            val invalid = { enterRecovery(confirming, retry = true) }
            if (confirming) active.confirmCreation(owned, invalid, ::received, ::failed)
            else active.restore(owned, invalid, ::received, ::failed)
            transferred = true
        } finally {
            // The view has already relinquished this copy. Own its erasure
            // through clock, rendering, callback allocation and handoff errors.
            // A successful handoff leaves cleanup with the bounded worker.
            if (!transferred) owned.fill('\u0000')
        }
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
        val previous = session
        session = null
        busy = false
        // Retire every owner even if cleanup fails. Clear secret views before
        // allocating the failure UI or its callback, preserving the first error
        // without allocating suppressed-exception storage.
        var failure: Throwable? = null
        try { clearSetupTimeout() }
        catch (problem: Throwable) { failure = problem }
        try { previous?.close() }
        catch (problem: Throwable) { if (failure == null) failure = problem }
        try { screens.clearSecrets() }
        catch (problem: Throwable) { if (failure == null) failure = problem }
        try { screens.failure(message, ::restart) }
        catch (problem: Throwable) { if (failure == null) failure = problem }
        if (failure != null) throw failure
    }

    private fun restart() {
        if (!resumed) return
        val previous = session
        session = null
        busy = true
        // Retire the old setup before allocating its replacement. A failed
        // cancellation or worker cleanup must not retain displayed secrets.
        var failure: Throwable? = null
        try { authentication.cancel() }
        catch (problem: Throwable) { failure = problem }
        try { clearSetupTimeout() }
        catch (problem: Throwable) { if (failure == null) failure = problem }
        try { previous?.close() }
        catch (problem: Throwable) { if (failure == null) failure = problem }
        try { screens.clearSecrets() }
        catch (problem: Throwable) { if (failure == null) failure = problem }
        if (failure != null) throw failure
        session = WalletPlatformSession(applicationContext, storage, mainExecutor)
        inspect()
    }

    private fun setupOpen(): Boolean {
        if ((setupWindow?.remainingMillis ?: 0) > 0) return true
        showFailure(R.string.setup_expired)
        return false
    }

    private fun startSetupTimeout(window: SetupWindow): Boolean {
        clearSetupTimeout()
        setupWindow = window
        val failure = try {
            queueSetupTimeout(window)
        } catch (_: Exception) {
            R.string.operation_failed
        } catch (problem: Throwable) {
            // Retire the session even if the timer was enqueued before the
            // error. Rendering failure must not replace the original error.
            try { showFailure(R.string.operation_failed) } catch (_: Throwable) { /* Preserve setup error. */ }
            throw problem
        }
        if (failure == 0) return true
        showFailure(failure)
        return false
    }

    // Return a failure message resource, or zero once cleanup is scheduled.
    private fun queueSetupTimeout(window: SetupWindow): Int {
        val remaining = window.remainingMillis
        if (remaining <= 0) return R.string.setup_expired
        val timeout = Runnable { showFailure(R.string.setup_expired) }
        setupTimeout = timeout
        return if (handler.postDelayed(timeout, remaining)) 0 else R.string.operation_failed
    }

    private fun clearSetupTimeout() {
        setupWindow = null
        setupTimeout?.let(handler::removeCallbacks)
        setupTimeout = null
    }
}
