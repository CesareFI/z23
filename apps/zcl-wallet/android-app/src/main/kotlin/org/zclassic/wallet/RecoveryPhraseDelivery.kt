// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import java.util.concurrent.Executor

/** One pending recovery phrase crossing from the worker to the UI. Closing
 * clears an undelivered phrase even if the UI queue never runs again. The UI
 * executor and close() belong to the main thread; posting may use the worker.
 * Once the UI claims a phrase, its receiver owns cleanup (including onPause).
 * No caller may reuse the transferred array. No callback runs under our lock.
 */
internal class RecoveryPhraseDelivery(private val ui: Executor) {
    private class Delivery(val words: CharArray, val receive: (CharArray) -> Unit)
    private val control = Any()
    private var closed = false
    private var pending: Delivery? = null

    fun post(ownedWords: CharArray, receive: (CharArray) -> Unit) {
        var accepted = false
        try {
            val delivery = Delivery(ownedWords, receive)
            synchronized(control) {
                if (closed) return
                check(pending == null) { "Recovery phrase delivery is already pending" }
                require(ownedWords.size <= 215) { "Recovery phrase delivery exceeds its bound" }
                pending = delivery
            }
            try {
                ui.execute { deliver(delivery) }
                accepted = true
            } finally {
                if (!accepted) discard(delivery)
            }
        } finally {
            if (!accepted) ownedWords.fill('\u0000')
        }
    }

    private fun discard(delivery: Delivery) = synchronized(control) {
        if (pending === delivery) {
            pending = null
            delivery.words.fill('\u0000')
        }
    }

    private fun deliver(delivery: Delivery) {
        synchronized(control) {
            if (pending !== delivery) return
            pending = null
        }
        var transferred = false
        try {
            delivery.receive(delivery.words)
            transferred = true
        } finally {
            if (!transferred) delivery.words.fill('\u0000')
        }
    }

    fun close() = synchronized(control) {
        closed = true
        pending?.words?.fill('\u0000')
        pending = null
    }
}
