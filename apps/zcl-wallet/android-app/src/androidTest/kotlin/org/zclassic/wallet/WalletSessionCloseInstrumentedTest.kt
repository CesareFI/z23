// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import java.io.File
import java.util.concurrent.CountDownLatch
import java.util.concurrent.ThreadPoolExecutor
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicReference
import javax.crypto.Cipher
import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith
import org.zclassic.wallet.core.Network
import org.zclassic.wallet.core.WalletStorage

/** Public lifetime fixture only. The cipher is never initialized or used;
 * no authentication, key creation, wallet read/write or custody bypass occurs. */
@RunWith(AndroidJUnit4::class)
class WalletSessionCloseInstrumentedTest {
    private fun field(type: Class<*>, name: String) = type.getDeclaredField(name).apply {
        isAccessible = true
    }

    private fun publicSetup(entropy: ByteArray): Any {
        val type = Class.forName("org.zclassic.wallet.WalletPlatformSession\$Setup")
        val constructor = type.getDeclaredConstructor(PreparedWalletAction::class.java,
            ByteArray::class.java, ByteArray::class.java).apply { isAccessible = true }
        val prepared = PreparedWalletAction(WalletAction.CREATE,
            Cipher.getInstance("AES/GCM/NoPadding"), Network.TESTNET)
        return constructor.newInstance(prepared, entropy, null)
    }

    @Test fun closingUsesItsPreparedFinalizerAndClearsOnlyAfterActiveWork() {
        val instrumentation = InstrumentationRegistry.getInstrumentation()
        val context = instrumentation.targetContext
        val directory = File(context.cacheDir, "unused-session-close-${System.nanoTime()}")
        assertFalse(directory.exists())
        val session = WalletPlatformSession(context, WalletStorage(directory.absolutePath), context.mainExecutor)
        val owner = field(WalletPlatformSession::class.java, "work").get(session) as OwnedExecutor
        val staged = field(WalletPlatformSession::class.java, "setup")
        // Capture the callbacks already owned at construction. A close-time
        // callback cannot satisfy this identity check, even if equivalent.
        val preparedCallbacks = WalletPlatformSession::class.java.declaredFields.mapNotNull {
            it.isAccessible = true
            it.get(session) as? Function0<*>
        }
        val entropy = ByteArray(32) { 0x42 } // Synthetic, unfunded marker only.
        val entered = CountDownLatch(1)
        val release = CountDownLatch(1)
        val failure = AtomicReference<Throwable>()
        var backend: ThreadPoolExecutor? = null
        try {
            assertTrue("Session has no prepared cleanup callback", preparedCallbacks.isNotEmpty())
            val setup = publicSetup(entropy)
            assertTrue(owner.submit {
                try {
                    staged.set(session, setup)
                    entered.countDown()
                    check(release.await(10, TimeUnit.SECONDS))
                } catch (problem: Throwable) { failure.set(problem) }
            })
            assertTrue(entered.await(5, TimeUnit.SECONDS))
            val pool = field(OwnedExecutor::class.java, "executor").get(owner) as ThreadPoolExecutor
            backend = pool
            val finalizer = field(pool.javaClass, "clearSession")
            instrumentation.runOnMainSync {
                session.close()
                assertTrue(owner.isClosed)
                val selected = finalizer.get(pool)
                assertTrue("Close allocated a replacement callback", preparedCallbacks.any { it === selected })
                assertSame(setup, staged.get(session))
                assertTrue("Close cleared active worker state", entropy.all { it == 0x42.toByte() })
                session.close()
                assertSame(selected, finalizer.get(pool))
            }
            release.countDown()
            assertTrue(pool.awaitTermination(5, TimeUnit.SECONDS))
            assertEquals(null, failure.get())
            assertNull(staged.get(session))
            assertNull(finalizer.get(pool))
            assertTrue(entropy.all { it == 0.toByte() })
            assertFalse(directory.exists())
        } finally {
            release.countDown()
            try {
                session.close()
                backend?.let { assertTrue(it.awaitTermination(5, TimeUnit.SECONDS)) }
            } finally { entropy.fill(0) }
        }
    }
}
