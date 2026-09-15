// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.app.Activity
import android.hardware.biometrics.BiometricManager
import android.hardware.biometrics.BiometricPrompt
import android.os.CancellationSignal
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import javax.crypto.Cipher
import org.zclassic.wallet.core.Network
import org.zclassic.wallet.core.WalletRecord
import org.zclassic.wallet.core.WrappingPolicy

internal enum class WalletAction { CREATE, RESTORE, UNLOCK }

/** Contains only a provider handle and public/ciphertext data, never entropy. */
internal class PreparedWalletAction(
    val action: WalletAction,
    val cipher: Cipher,
    val network: Network,
    val record: WalletRecord? = null,
    val encodedRecord: ByteArray? = null,
)

/** Main-thread platform prompt owner. No secret is generated/decrypted while
 * waiting for authentication. Credential UI may pause the activity, so a
 * successful result is delivered only after onResume, within a 90-second bound.
 */
internal class WalletAuthentication(
    private val activity: Activity,
    private val approved: (PreparedWalletAction) -> Unit,
    private val failed: () -> Unit,
) {
    private class Pending(val prepared: PreparedWalletAction) {
        val startedMillis = SystemClock.elapsedRealtime()
        val signal = CancellationSignal()
        var succeeded = false
        var timeout: Runnable? = null
    }

    private val handler = Handler(Looper.getMainLooper())
    private var pending: Pending? = null
    private var foreground = false
    private var deferredFailure = false
    val hasPending: Boolean get() = pending != null || deferredFailure

    fun begin(prepared: PreparedWalletAction) {
        cancel()
        val request = Pending(prepared)
        pending = request
        val started = try {
            start(request)
        } catch (_: RuntimeException) {
            false
        } catch (problem: Throwable) {
            // A timeout can be enqueued before setup throws. Retire the
            // request even if reporting failure also throws; preserve Error.
            try { fail(request) } catch (_: Throwable) { /* Preserve the setup error. */ }
            throw problem
        }
        if (!started) fail(request)
    }

    private fun start(request: Pending): Boolean {
        val timeout = Runnable { fail(request) }.also { request.timeout = it }
        if (!windowOpen(request) || !handler.postDelayed(timeout, WrappingPolicy.authenticationWindowMillis))
            return false
        prompt().authenticate(BiometricPrompt.CryptoObject(request.prepared.cipher), request.signal,
            activity.mainExecutor, callback(request))
        return true
    }

    private fun prompt(): BiometricPrompt = BiometricPrompt.Builder(activity)
        .setTitle(activity.getString(R.string.authenticate_title))
        .setSubtitle(activity.getString(R.string.authenticate_subtitle))
        .setAllowedAuthenticators(BiometricManager.Authenticators.BIOMETRIC_STRONG or
            BiometricManager.Authenticators.DEVICE_CREDENTIAL)
        .setConfirmationRequired(true)
        .build()

    private fun callback(request: Pending) = object : BiometricPrompt.AuthenticationCallback() {
        override fun onAuthenticationSucceeded(result: BiometricPrompt.AuthenticationResult) {
            if (pending !== request) return
            if (!windowOpen(request)) { fail(request); return }
            if (result.cryptoObject?.cipher !== request.prepared.cipher) {
                fail(request)
                return
            }
            request.succeeded = true
            deliver()
        }

        override fun onAuthenticationError(errorCode: Int, errString: CharSequence) {
            // Provider text is not displayed or logged by the wallet.
            fail(request)
        }
    }

    private fun fail(request: Pending) {
        if (pending !== request) return
        pending = null
        request.timeout?.let(handler::removeCallbacks)
        request.signal.cancel()
        if (foreground) failed() else deferredFailure = true
    }

    private fun deliver() {
        val request = pending ?: return
        if (!foreground || !request.succeeded) return
        if (!windowOpen(request)) { fail(request); return }
        pending = null
        request.timeout?.let(handler::removeCallbacks)
        approved(request.prepared)
    }

    private fun windowOpen(request: Pending): Boolean =
        WrappingPolicy.authenticationWindowOpen(request.startedMillis, SystemClock.elapsedRealtime())

    fun onResume() {
        foreground = true
        val request = pending
        if (request != null && !windowOpen(request)) { fail(request); return }
        if (deferredFailure) {
            deferredFailure = false
            failed()
        } else {
            deliver()
        }
    }

    fun onPause() { foreground = false }

    fun cancel() {
        val request = pending
        pending = null
        deferredFailure = false
        request?.timeout?.let(handler::removeCallbacks)
        request?.signal?.cancel()
    }
}
