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

internal enum class WalletProblem { STORAGE, PROTECTION, OPERATION }

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
        val entropy: ByteArray? = null,
        val header: ByteArray? = null,
    ) {
        fun clear() { entropy?.fill(0) }
    }

    private val work = OwnedExecutor()
    private val phrases = RecoveryPhraseDelivery(ui)
    private val wrappingKey = KeystoreWrappingKey(context, storage)
    private var setup: Setup? = null // Accessed only by the worker.

    fun close() {
        phrases.close()
        work.close(::clearSetup)
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
        if (!accepted) post { failure(problem) }
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

    fun createAfterAuthentication(prepared: PreparedWalletAction, words: (CharArray) -> Unit,
                                  failure: (WalletProblem) -> Unit) {
        launch(WalletProblem.OPERATION, failure) {
            clearSetup()
            check(prepared.action == WalletAction.CREATE)
            check(storage.read().status == CoreStatus.NOT_FOUND)
            val entropy = WalletKeys.createEntropy()
            var retained = false
            try {
                val header = WalletRecord.createHeader(entropy, prepared.network)
                setup = Setup(prepared, entropy, header)
                retained = true
                postWords(WalletKeys.recoveryPhrase(entropy), words)
            } finally {
                if (!retained) entropy.fill(0)
            }
        }
    }

    fun restoreAfterAuthentication(prepared: PreparedWalletAction, ready: () -> Unit,
                                   failure: (WalletProblem) -> Unit) {
        launch(WalletProblem.OPERATION, failure) {
            clearSetup()
            check(prepared.action == WalletAction.RESTORE)
            check(storage.read().status == CoreStatus.NOT_FOUND)
            setup = Setup(prepared)
            post(ready)
        }
    }

    fun confirmCreation(ownedPhrase: CharArray, mismatch: () -> Unit,
                        ready: (TransparentAddress) -> Unit, failure: (WalletProblem) -> Unit) {
        launch(WalletProblem.OPERATION, failure, { ownedPhrase.fill('\u0000') }) {
            val current = checkNotNull(setup)
            val entropy = checkNotNull(current.entropy)
            if (!WalletKeys.confirmRecoveryPhrase(entropy, ownedPhrase)) {
                post(mismatch)
                return@launch
            }
            val address = seal(current, entropy, checkNotNull(current.header))
            post { ready(address) }
        }
    }

    fun restore(ownedPhrase: CharArray, invalid: () -> Unit,
                ready: (TransparentAddress) -> Unit, failure: (WalletProblem) -> Unit) {
        launch(WalletProblem.OPERATION, failure, { ownedPhrase.fill('\u0000') }) {
            val current = checkNotNull(setup)
            check(current.prepared.action == WalletAction.RESTORE)
            val entropy = try {
                WalletKeys.restoreEntropy(ownedPhrase)
            } catch (_: IllegalArgumentException) {
                post(invalid)
                return@launch
            }
            try {
                val header = WalletRecord.createHeader(entropy, current.prepared.network)
                val address = seal(current, entropy, header)
                post { ready(address) }
            } finally {
                entropy.fill(0)
            }
        }
    }

    private fun seal(current: Setup, entropy: ByteArray, header: ByteArray): TransparentAddress {
        try {
            val cipher = current.prepared.cipher
            val parameters = checkNotNull(cipher.parameters).getParameterSpec(GCMParameterSpec::class.java)
            check(parameters.tLen == 128 && parameters.iv.size == 12)
            cipher.updateAAD(header)
            val encoded = WalletRecord.pack(header, parameters.iv, cipher.doFinal(entropy))
            check(commitPreparedWallet(storage, current.prepared.action, encoded, entropy) == CoreStatus.OK) {
                "Wallet commit requires recovery"
            }
            val stored = storage.read()
            check(stored.status == CoreStatus.OK && !stored.pending && encoded.contentEquals(stored.record)) {
                "Stored wallet requires verification"
            }
            // Fresh creation uses entropy we generated/confirmed in this
            // session. Stored-wallet unlock below requires GCM decryption first.
            return WalletKeys.receivingAddress(entropy, current.prepared.network)
        } finally {
            clearSetup()
        }
    }

    fun unlockAfterAuthentication(prepared: PreparedWalletAction, ready: (TransparentAddress) -> Unit,
                                  failure: (WalletProblem) -> Unit) {
        launch(WalletProblem.OPERATION, failure) {
            clearSetup()
            check(prepared.action == WalletAction.UNLOCK)
            val record = checkNotNull(prepared.record)
            prepared.cipher.updateAAD(record.header)
            val entropy = prepared.cipher.doFinal(record.ciphertext)
            try {
                val address = WalletRecord.recoveredAddress(record.header, entropy, record.network)
                // Also checks an existing committed record still equals the
                // authenticated bytes; pending promotion follows GCM and C checks.
                check(storage.promote(checkNotNull(prepared.encodedRecord)) == CoreStatus.OK)
                post { ready(address) }
            } finally {
                entropy.fill(0)
            }
        }
    }
}
