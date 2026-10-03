// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.content.Context
import java.util.concurrent.Executor
import javax.crypto.spec.GCMParameterSpec
import org.zclassic.wallet.core.CoreStatus
import org.zclassic.wallet.core.Network
import org.zclassic.wallet.core.TransparentAddress
import org.zclassic.wallet.core.WalletKeys
import org.zclassic.wallet.core.WalletRecord
import org.zclassic.wallet.core.WalletStorage

internal enum class WalletProblem { STORAGE, PROTECTION, OPERATION, RESOURCES }

/** Platform action routing only. Restored entropy may have consumed historical
 * change indexes, so restoration must not initialize a fresh counter. */
internal fun commitPreparedWallet(storage: WalletStorage, action: WalletAction,
                                  encoded: ByteArray, entropy: ByteArray): CoreStatus = when (action) {
    WalletAction.CREATE -> storage.createFreshWithChange(encoded, entropy)
    WalletAction.RESTORE -> storage.create(encoded)
    WalletAction.UNLOCK -> CoreStatus.INVALID_ARGUMENT
}

/** One foreground UI session's platform work. C owns wallet validation, key
 * derivation, confirmation and persistence. Entropy is held here only while
 * preparing the platform GCM input; the worker is its sole managed owner.
 */
internal class WalletPlatformSession(
    context: Context,
    private val storage: WalletStorage,
    private val ui: Executor,
) {
    private class Setup(
        val prepared: PreparedWalletAction,
        val window: SetupWindow,
        val entropy: ByteArray? = null,
        val header: ByteArray? = null,
    ) {
        fun clear() { entropy?.fill(0) }
    }

    private val work = OwnedExecutor()
    private val phrases = RecoveryPhraseDelivery(ui)
    private val wrappingKey = KeystoreWrappingKey(context, storage)
    // Worker-owned until pool termination. Final cleanup starts only when no
    // worker can access setup; a rejected session has only its empty state.
    private var setup: Setup? = null
    // Allocate the bound callback while construction still owns no secret or
    // admitted worker. close() must not need this allocation before shutdown.
    private val finishSession: () -> Unit = ::clearSetup

    fun close() {
        phrases.close()
        work.close(finishSession)
    }

    private fun clearSetup() {
        val previous = setup
        setup = null
        previous?.clear()
    }

    private fun post(action: () -> Unit) {
        ui.execute { if (!work.isClosed) action() }
    }

    private fun launch(problem: WalletProblem, failure: (WalletProblem) -> Unit,
                       cleanup: () -> Unit = {}, action: () -> Unit) {
        val accepted = work.submit(cleanup) {
            var completed = false
            try {
                action()
                completed = true
            } catch (_: Exception) {
                clearSetup()
                post { failure(problem) }
            } finally {
                // Also clear owned entropy before an Error (including VM
                // allocation failure) propagates; do not swallow fatal errors.
                if (!completed || work.isClosed) clearSetup()
            }
        }
        if (!accepted) post { failure(WalletProblem.RESOURCES) }
    }

    fun inspect(result: (WalletStorage.ReadResult) -> Unit, failure: (WalletProblem) -> Unit) {
        launch(WalletProblem.STORAGE, failure) {
            val read = storage.read()
            post { result(read) }
        }
    }

    fun prepare(action: WalletAction, network: Network,
                result: (PreparedWalletAction) -> Unit, failure: (WalletProblem) -> Unit) {
        launch(WalletProblem.PROTECTION, failure) {
            clearSetup()
            check(action != WalletAction.UNLOCK) { "Unlock requires a stored record" }
            val prepared = PreparedWalletAction(action, wrappingKey.prepareCreation(), network)
            post { result(prepared) }
        }
    }

    fun prepareUnlock(encoded: ByteArray, result: (PreparedWalletAction) -> Unit,
                      failure: (WalletProblem) -> Unit) {
        launch(WalletProblem.PROTECTION, failure) {
            clearSetup()
            val record = WalletRecord.parse(encoded)
            val prepared = PreparedWalletAction(WalletAction.UNLOCK, wrappingKey.prepareUnlock(record),
                record.network, record, encoded.copyOf())
            post { result(prepared) }
        }
    }

    private fun postWords(words: CharArray, result: (CharArray) -> Unit) {
        phrases.post(words, result)
    }

    fun createAfterAuthentication(prepared: PreparedWalletAction, window: SetupWindow, words: (CharArray) -> Unit,
                                  failure: (WalletProblem) -> Unit) {
        launch(WalletProblem.OPERATION, failure) {
            clearSetup()
            window.requireOpen()
            check(prepared.action == WalletAction.CREATE)
            check(storage.read().status == CoreStatus.NOT_FOUND)
            val entropy = WalletKeys.createEntropy()
            var retained = false
            try {
                val header = WalletRecord.createHeader(entropy, prepared.network)
                window.requireOpen()
                setup = Setup(prepared, window, entropy, header)
                retained = true
                postWords(WalletKeys.recoveryPhrase(entropy), words)
            } finally {
                if (!retained) entropy.fill(0)
            }
        }
    }

    fun restoreAfterAuthentication(prepared: PreparedWalletAction, window: SetupWindow, ready: () -> Unit,
                                   failure: (WalletProblem) -> Unit) {
        launch(WalletProblem.OPERATION, failure) {
            clearSetup()
            window.requireOpen()
            check(prepared.action == WalletAction.RESTORE)
            check(storage.read().status == CoreStatus.NOT_FOUND)
            window.requireOpen()
            setup = Setup(prepared, window)
            post(ready)
        }
    }

    fun confirmCreation(ownedPhrase: CharArray, mismatch: () -> Unit,
                        ready: (TransparentAddress) -> Unit, failure: (WalletProblem) -> Unit) {
        launch(WalletProblem.OPERATION, failure, { ownedPhrase.fill('\u0000') }) {
            val current = checkNotNull(setup)
            current.window.requireOpen()
            val entropy = checkNotNull(current.entropy)
            // Retire words at their last use; later encryption and disk IO
            // need only entropy. Submission cleanup still covers cancellation.
            val confirmed = try { WalletKeys.confirmRecoveryPhrase(entropy, ownedPhrase) }
                finally { ownedPhrase.fill('\u0000') }
            if (!confirmed) {
                post(mismatch)
                return@launch
            }
            val address = seal(current, entropy, checkNotNull(current.header))
            postSealed(current.window, address, ready, failure)
        }
    }

    fun restore(ownedPhrase: CharArray, invalid: () -> Unit,
                ready: (TransparentAddress) -> Unit, failure: (WalletProblem) -> Unit) {
        launch(WalletProblem.OPERATION, failure, { ownedPhrase.fill('\u0000') }) {
            val current = checkNotNull(setup)
            current.window.requireOpen()
            check(current.prepared.action == WalletAction.RESTORE)
            val entropy = try {
                try { WalletKeys.restoreEntropy(ownedPhrase) }
                finally { ownedPhrase.fill('\u0000') }
            } catch (_: IllegalArgumentException) {
                post(invalid)
                return@launch
            }
            try {
                val header = WalletRecord.createHeader(entropy, current.prepared.network)
                val address = seal(current, entropy, header)
                postSealed(current.window, address, ready, failure)
            } finally {
                entropy.fill(0)
            }
        }
    }

    private fun postSealed(window: SetupWindow, address: TransparentAddress,
                           ready: (TransparentAddress) -> Unit, failure: (WalletProblem) -> Unit) {
        // Capture only the public window/address, after seal retired entropy.
        // A delayed result cannot cancel the timeout and reopen expired setup.
        post {
            if (window.remainingMillis > 0) ready(address) else failure(WalletProblem.OPERATION)
        }
    }

    private fun seal(current: Setup, entropy: ByteArray, header: ByteArray): TransparentAddress {
        try {
            current.window.requireOpen()
            val cipher = current.prepared.cipher
            val parameters = checkNotNull(cipher.parameters).getParameterSpec(GCMParameterSpec::class.java)
            check(parameters.tLen == 128 && parameters.iv.size == 12)
            cipher.updateAAD(header)
            val encoded = WalletRecord.pack(header, parameters.iv, cipher.doFinal(entropy))
            // Finish the public result before persistence. Restoration writes
            // ciphertext only; fresh creation still needs entropy to establish
            // its authenticated initial change state in C.
            val address = WalletKeys.receivingAddress(entropy, current.prepared.network)
            if (current.prepared.action == WalletAction.RESTORE) entropy.fill(0)
            // Expiry or foreground closure during provider work must refuse
            // new persistence. The closed-state read is its admission point;
            // closure afterward lets C finish its bounded durability protocol.
            current.window.requireOpen()
            check(!work.isClosed) { "Wallet session closed" }
            check(commitPreparedWallet(storage, current.prepared.action, encoded, entropy) == CoreStatus.OK) {
                "Wallet commit requires recovery"
            }
            // No plaintext is needed by record readback or UI delivery.
            entropy.fill(0)
            clearSetup()
            val stored = storage.read()
            check(stored.status == CoreStatus.OK && !stored.pending && encoded.contentEquals(stored.record)) {
                "Stored wallet requires verification"
            }
            return address
        } finally {
            // Restored entropy is local to restore(), outside Setup. Retire
            // both forms here before the public result reaches UI scheduling.
            entropy.fill(0)
            clearSetup()
        }
    }

    fun unlockAfterAuthentication(prepared: PreparedWalletAction, ready: (TransparentAddress) -> Unit,
                                  failure: (WalletProblem) -> Unit, window: UnlockWindow = UnlockWindow()) {
        launch(WalletProblem.OPERATION, failure) {
            clearSetup()
            window.requireOpen()
            check(prepared.action == WalletAction.UNLOCK)
            val record = checkNotNull(prepared.record)
            prepared.cipher.updateAAD(record.header)
            // C-parsed ciphertext contains 16..32 entropy bytes and a 16-byte
            // GCM tag. Own the bounded destination before the provider writes;
            // a failed partial decrypt must still reach our complete wipe.
            val capacity = record.ciphertext.size - 16
            check(capacity in 16..32)
            val entropy = ByteArray(capacity)
            val address = try {
                val written = prepared.cipher.doFinal(record.ciphertext, 0, record.ciphertext.size, entropy, 0)
                check(written == entropy.size) { "Invalid decrypted wallet length" }
                WalletRecord.recoveredAddress(record.header, entropy, record.network)
            } finally {
                // Address verification consumes the last plaintext use.
                // Storage durability and UI scheduling need only public data.
                entropy.fill(0)
            }
            // Also checks an existing committed record still equals the
            // authenticated bytes; pending promotion follows GCM and C checks.
            // Closing during decryption cannot admit a new promotion. Once
            // admitted here, C must finish its existing durability protocol.
            window.requireOpen()
            check(!work.isClosed) { "Wallet session closed" }
            check(storage.promote(checkNotNull(prepared.encodedRecord)) == CoreStatus.OK)
            post {
                // Durability already completed; an expired UI delivery must
                // require another unlock, never undo the committed record.
                if (window.isOpen) ready(address) else failure(WalletProblem.OPERATION)
            }
        }
    }
}
