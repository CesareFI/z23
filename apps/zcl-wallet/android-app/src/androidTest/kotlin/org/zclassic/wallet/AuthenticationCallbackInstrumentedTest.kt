// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.app.Activity
import android.hardware.biometrics.BiometricPrompt
import android.os.CancellationSignal
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import java.util.concurrent.atomic.AtomicReference
import javax.crypto.Cipher
import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith
import org.zclassic.wallet.core.Network

/** Synthetic callback routing only. Requests contain uninitialized cipher
 * handles and the receiver only counts deliveries. No prompt, key, entropy,
 * wallet session or storage operation is created, and no framework policy is
 * changed. Success enters the private handler with the nullable cipher that
 * the framework callback forwards; errors enter the actual callback object.
 * This does not exercise framework result delivery or establish successful
 * hardware authentication. */
@RunWith(AndroidJUnit4::class)
class AuthenticationCallbackInstrumentedTest {
    private fun onMain(action: () -> Unit) {
        val failure = AtomicReference<Throwable?>()
        InstrumentationRegistry.getInstrumentation().runOnMainSync {
            try { action() } catch (problem: Throwable) { failure.set(problem) }
        }
        failure.get()?.let { throw it }
    }

    private fun field(name: String) = WalletAuthentication::class.java.getDeclaredField(name).apply {
        isAccessible = true
    }

    private fun inertCipher(): Cipher = Cipher.getInstance("AES/GCM/NoPadding")

    private inner class Fixture(notificationFailure: Throwable? = null) : AutoCloseable {
        val approved = mutableListOf<PreparedWalletAction>()
        var failed = 0
        val auth = WalletAuthentication(Activity(), { approved.add(it) }, {
            failed++
            notificationFailure?.let { throw it }
        })
        private val requestType = WalletAuthentication::class.java.declaredClasses.single { it.simpleName == "Pending" }
        private val constructor = requestType.getDeclaredConstructor(PreparedWalletAction::class.java).apply {
            isAccessible = true
        }
        private val callback = WalletAuthentication::class.java.getDeclaredMethod("callback", requestType).apply {
            isAccessible = true
        }
        private val succeed = WalletAuthentication::class.java.getDeclaredMethod("succeed", requestType,
            Cipher::class.java).apply { isAccessible = true }

        inner class Request(val identity: Any, val prepared: PreparedWalletAction) {
            val events = callback.invoke(auth, identity) as BiometricPrompt.AuthenticationCallback
            val signal = requestType.getDeclaredField("signal").apply { isAccessible = true }
                .get(identity) as CancellationSignal

            fun success(cipher: Cipher? = prepared.cipher) {
                succeed.invoke(auth, identity, cipher)
            }

            fun error() = events.onAuthenticationError(BiometricPrompt.BIOMETRIC_ERROR_CANCELED,
                "Public synthetic provider cancellation")

            fun invalidateOrigin() {
                requestType.getDeclaredField("startedMillis").apply { isAccessible = true }.setLong(identity, -1)
            }
        }

        fun install(cipher: Cipher = inertCipher()): Request {
            auth.cancel()
            val prepared = PreparedWalletAction(WalletAction.CREATE, cipher, Network.TESTNET)
            val request = constructor.newInstance(prepared)
            field("pending").set(auth, request)
            return Request(request, prepared)
        }

        fun assertPending(request: Request) {
            assertTrue(auth.hasPending)
            assertSame(request.identity, field("pending").get(auth))
            assertFalse(request.signal.isCanceled)
        }

        override fun close() { auth.cancel() }
    }

    @Test fun matchingCipherDeliversTheExactRequestOnlyOnce() = onMain {
        Fixture().use { fixture ->
            fixture.auth.onResume()
            val request = fixture.install()
            request.success()
            assertEquals(listOf(request.prepared), fixture.approved)
            assertFalse(fixture.auth.hasPending)
            request.success()
            request.error()
            fixture.auth.onPause()
            fixture.auth.onResume()
            assertEquals(listOf(request.prepared), fixture.approved)
            assertEquals(0, fixture.failed)
        }
    }

    @Test fun missingOrDifferentCipherRefusesAndRetiresTheRequest() = onMain {
        Fixture().use { fixture ->
            fixture.auth.onResume()
            for ((index, cipher) in listOf(null, inertCipher()).withIndex()) {
                val request = fixture.install()
                request.success(cipher)
                assertTrue(fixture.approved.isEmpty())
                assertEquals(index + 1, fixture.failed)
                assertFalse(fixture.auth.hasPending)
                assertTrue(request.signal.isCanceled)
                request.success()
                request.error()
                assertEquals(index + 1, fixture.failed)
            }
        }
    }

    @Test fun successfulBackgroundCallbackWaitsForForegroundExactlyOnce() = onMain {
        Fixture().use { fixture ->
            fixture.auth.onResume()
            val request = fixture.install()
            fixture.auth.onPause()
            request.success()
            assertTrue(fixture.approved.isEmpty())
            fixture.assertPending(request)
            fixture.auth.onResume()
            fixture.auth.onPause()
            fixture.auth.onResume()
            assertEquals(listOf(request.prepared), fixture.approved)
            assertEquals(0, fixture.failed)
            assertFalse(fixture.auth.hasPending)
        }
    }

    @Test fun backgroundFailureRetiresImmediatelyAndReportsOnlyOnResume() = onMain {
        Fixture().use { fixture ->
            val request = fixture.install()
            request.error()
            assertTrue(request.signal.isCanceled)
            assertNull(field("pending").get(fixture.auth))
            assertTrue(fixture.auth.hasPending)
            assertEquals(0, fixture.failed)
            request.success()
            request.error()
            fixture.auth.onResume()
            fixture.auth.onPause()
            fixture.auth.onResume()
            assertTrue(fixture.approved.isEmpty())
            assertEquals(1, fixture.failed)
            assertFalse(fixture.auth.hasPending)
        }
    }

    @Test fun retiredCallbacksCannotChangeAReplacementEvenWithTheSameCipher() = onMain {
        Fixture().use { fixture ->
            fixture.auth.onResume()
            val old = fixture.install()
            val current = fixture.install(old.prepared.cipher)
            assertTrue(old.signal.isCanceled)
            old.success()
            old.error()
            assertTrue(fixture.approved.isEmpty())
            assertEquals(0, fixture.failed)
            fixture.assertPending(current)
            current.success()
            old.error()
            assertEquals(listOf(current.prepared), fixture.approved)
            assertEquals(0, fixture.failed)
            assertFalse(fixture.auth.hasPending)
        }
    }

    @Test fun throwingCancellationStillReportsForegroundFailureOnce() = onMain {
        for (fatal in listOf(false, true)) {
            val primary = if (fatal) OutOfMemoryError("Public cancellation failure")
                else IllegalStateException("Public cancellation failure")
            val secondary = IllegalStateException("Public notification failure")
            for (notificationFailure in listOf(null, secondary)) {
                Fixture(notificationFailure).use { fixture ->
                    fixture.auth.onResume()
                    val request = fixture.install()
                    request.signal.setOnCancelListener { throw primary }
                    assertSame(primary, assertThrows(Throwable::class.java) { request.error() })
                    assertTrue(request.signal.isCanceled)
                    assertFalse(fixture.auth.hasPending)
                    assertEquals(1, fixture.failed)
                    request.error()
                    request.success()
                    fixture.auth.onPause()
                    fixture.auth.onResume()
                    assertEquals(1, fixture.failed)
                    assertTrue(fixture.approved.isEmpty())
                }
            }
        }
    }

    @Test fun throwingCancellationStillDefersBackgroundFailureUntilResume() = onMain {
        for (fatal in listOf(false, true)) {
            val primary = if (fatal) OutOfMemoryError("Public cancellation failure")
                else IllegalStateException("Public cancellation failure")
            Fixture().use { fixture ->
                val request = fixture.install()
                request.signal.setOnCancelListener { throw primary }
                assertSame(primary, assertThrows(Throwable::class.java) { request.error() })
                assertTrue(request.signal.isCanceled)
                assertNull(field("pending").get(fixture.auth))
                assertTrue(fixture.auth.hasPending)
                assertEquals(0, fixture.failed)
                request.error()
                request.success()
                fixture.auth.onResume()
                fixture.auth.onPause()
                fixture.auth.onResume()
                assertEquals(1, fixture.failed)
                assertFalse(fixture.auth.hasPending)
                assertTrue(fixture.approved.isEmpty())
            }
        }
    }

    @Test fun cancellationMakesEveryLateCallbackInert() = onMain {
        Fixture().use { fixture ->
            fixture.auth.onResume()
            val request = fixture.install()
            fixture.auth.cancel()
            request.success()
            request.error()
            fixture.auth.onPause()
            fixture.auth.onResume()
            assertTrue(request.signal.isCanceled)
            assertTrue(fixture.approved.isEmpty())
            assertEquals(0, fixture.failed)
            assertFalse(fixture.auth.hasPending)
        }
    }

    @Test fun invalidOriginRefusesEvenAMatchingSuccessfulCallback() = onMain {
        Fixture().use { fixture ->
            fixture.auth.onResume()
            val request = fixture.install()
            request.invalidateOrigin()
            request.success()
            assertTrue(request.signal.isCanceled)
            assertTrue(fixture.approved.isEmpty())
            assertEquals(1, fixture.failed)
            assertFalse(fixture.auth.hasPending)
        }
    }

    @Test fun deferredSuccessRechecksItsOriginAtForegroundDelivery() = onMain {
        Fixture().use { fixture ->
            val request = fixture.install()
            request.success()
            assertTrue(fixture.approved.isEmpty())
            fixture.assertPending(request)
            request.invalidateOrigin()
            fixture.auth.onResume()
            assertTrue(request.signal.isCanceled)
            assertTrue(fixture.approved.isEmpty())
            assertEquals(1, fixture.failed)
            assertFalse(fixture.auth.hasPending)
        }
    }
}
