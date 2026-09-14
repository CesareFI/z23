// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.os.Build
import android.os.ParcelFileDescriptor
import android.os.SystemClock
import android.text.TextUtils
import android.text.Editable
import android.text.TextWatcher
import android.view.View
import android.view.WindowManager
import android.view.accessibility.AccessibilityNodeInfo
import android.widget.TextView
import androidx.lifecycle.Lifecycle
import androidx.test.core.app.ActivityScenario
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import java.io.File
import java.lang.reflect.InvocationTargetException
import java.nio.file.Files
import java.nio.file.LinkOption
import java.security.KeyStore
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import java.util.concurrent.ThreadPoolExecutor
import java.util.concurrent.atomic.AtomicBoolean
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertSame
import org.junit.Assert.assertThrows
import org.junit.Assert.assertTrue
import org.junit.Assume.assumeTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.zclassic.wallet.core.CoreStatus
import org.zclassic.wallet.core.Network
import org.zclassic.wallet.core.WalletKeys
import org.zclassic.wallet.core.WalletStorage

/** Explicitly opted-in acceptance on a fresh, isolated emulator using its
 * PUBLIC test PIN. Never run on an operator wallet or a physical user device.
 * No seed/phrase/screenshot/hierarchy is written to test output or a file.
 */
@RunWith(AndroidJUnit4::class)
class WalletFlowInstrumentedTest {
    private val instrumentation = InstrumentationRegistry.getInstrumentation()
    private val directory = File(instrumentation.targetContext.noBackupFilesDir, "wallet-v1")
    private val alias = "org.zclassic.wallet.wrap.v1"
    private var scenario: ActivityScenario<MainActivity>? = null
    private var activity: MainActivity? = null
    private var ownsFixture = false
    private var existingDirectory = false
    private var existingLock = false

    private fun store() = KeyStore.getInstance("AndroidKeyStore").apply { load(null) }

    @Before fun isolatedEmulatorOnly() {
        assumeTrue("Interactive test requires explicit emulator-fixture opt-in",
            InstrumentationRegistry.getArguments().getString("walletFixture") == "yes")
        assertEquals("ranchu", Build.HARDWARE)
        assertEquals("org.zclassic.wallet.dev", instrumentation.targetContext.packageName)
        existingDirectory = directory.exists()
        if (existingDirectory) {
            assertTrue(Files.isDirectory(directory.toPath(), LinkOption.NOFOLLOW_LINKS))
            val existing = checkNotNull(directory.listFiles())
            // The C read path creates an empty directory/lock even before a
            // wallet exists. Preserve that scaffold; refuse ANY record/data.
            assertTrue("Refusing to touch existing wallet files", existing.all {
                it.name == ".lock" && it.length() == 0L &&
                    Files.isRegularFile(it.toPath(), LinkOption.NOFOLLOW_LINKS)
            })
            existingLock = existing.isNotEmpty()
        }
        assertFalse("Refusing to touch an existing wrapping key", store().containsAlias(alias))
        ownsFixture = true
        scenario = ActivityScenario.launch(MainActivity::class.java).also { launched ->
            launched.onActivity { activity = it }
        }
        awaitView(R.id.create_wallet)
        onUi {
            assertTrue(it.window.attributes.flags and WindowManager.LayoutParams.FLAG_SECURE != 0)
        }
    }

    @After fun removeOnlyThisInvocationFixture() {
        scenario?.close()
        if (!ownsFixture) return
        // The activity's close is nonblocking. Wait for its worker cleanup
        // before deleting only the test's newly created, fixed-name files.
        awaitCondition("Wallet worker did not finish cleanup") {
            Thread.getAllStackTraces().keys.none { it.name == "WalletPlatform" && it.isAlive }
        }
        if (directory.exists()) {
            val files = checkNotNull(directory.listFiles())
            val names = setOf("wallet.zcl", ".wallet.pending", ".change.index", ".lock")
            assertTrue("Unexpected file in isolated fixture", files.all { it.isFile && it.name in names })
            for (file in files) {
                if (file.name != ".lock" || !existingLock)
                    assertTrue("Test-file cleanup failed", file.delete())
            }
            if (!existingDirectory) assertTrue("Test-directory cleanup failed", directory.delete())
        }
        val keys = store()
        if (keys.containsAlias(alias)) keys.deleteEntry(alias)
        assertFalse(keys.containsAlias(alias))
    }

    private fun onUi(action: (MainActivity) -> Unit) {
        instrumentation.runOnMainSync { action(checkNotNull(activity)) }
    }

    private fun awaitCondition(message: String, condition: () -> Boolean) {
        val deadline = SystemClock.uptimeMillis() + 45_000
        while (SystemClock.uptimeMillis() < deadline) {
            if (condition()) return
            SystemClock.sleep(50)
        }
        throw AssertionError(message)
    }

    private fun awaitView(id: Int) = awaitCondition("Expected wallet screen was not reached") {
        var visible = false
        onUi { visible = it.findViewById<View>(id)?.isShown == true }
        visible
    }

    private fun click(id: Int) {
        awaitView(id)
        onUi { assertTrue(checkNotNull(it.findViewById<View>(id)).performClick()) }
    }

    @Suppress("DEPRECATION") // Recycle pooled API-30 nodes; later APIs ignore it.
    private fun credentialEntryVisible(): Boolean {
        val root = instrumentation.uiAutomation.rootInActiveWindow ?: return false
        val queue = ArrayDeque<AccessibilityNodeInfo>()
        queue.add(root)
        var found = false
        var visited = 0
        try {
            while (queue.isNotEmpty() && visited++ < 128) {
                val node = queue.removeFirst()
                val system = node.packageName == "com.android.settings" || node.packageName == "com.android.systemui"
                if (system && node.isPassword && node.isVisibleToUser) found = true
                for (index in 0 until node.childCount.coerceAtMost(32)) {
                    node.getChild(index)?.let(queue::addLast)
                }
                node.recycle()
            }
        } finally {
            for (node in queue) node.recycle()
        }
        return found
    }

    private fun shellPublicCommand(command: String) {
        ParcelFileDescriptor.AutoCloseInputStream(instrumentation.uiAutomation.executeShellCommand(command)).use {
            val output = ByteArray(1024)
            var total = 0
            while (true) {
                val count = it.read(output)
                if (count < 0) break
                total += count
                check(total <= 1024) { "Unexpected command output" }
            }
        }
    }

    private fun authenticate() {
        awaitCondition("Device credential prompt did not appear") {
            onUi {
                val status = it.findViewById<TextView>(R.id.status_message)?.text
                check(status != it.getString(R.string.protection_failed)) { "Platform preparation refused the emulator" }
                check(status != it.getString(R.string.authentication_failed)) { "Platform authentication failed" }
            }
            credentialEntryVisible()
        }
        // This emulator's documented public test-only lock PIN, never a user PIN.
        shellPublicCommand("input text 244680")
        shellPublicCommand("input keyevent 66")
    }

    private fun enter(phrase: CharArray) {
        awaitView(R.id.recovery_input)
        onUi {
            val input = checkNotNull(it.findViewById<RecoveryInputView>(R.id.recovery_input))
            for (character in phrase) input.append(character)
        }
        click(R.id.save_wallet)
    }

    private fun address(): String {
        awaitView(R.id.receiving_address)
        var address = ""
        onUi { address = it.findViewById<TextView>(R.id.receiving_address).text.toString() }
        return address // Public receiving address only.
    }

    @Test fun unsupportedKeystoreRefusesSetupWithoutCreatingWallet() {
        assumeTrue("This case requires the explicit software-only emulator fixture",
            InstrumentationRegistry.getArguments().getString("softwareKeystoreFixture") == "yes")
        click(R.id.create_wallet)
        awaitView(R.id.retry_wallet)
        onUi {
            assertEquals(it.getString(R.string.protection_failed),
                it.findViewById<TextView>(R.id.status_message).text.toString())
            assertTrue(it.findViewById<View>(R.id.recovery_words) == null)
            assertTrue(it.findViewById<View>(R.id.recovery_input) == null)
        }
        assertEquals(CoreStatus.NOT_FOUND, WalletStorage(directory.absolutePath).read().status)
        // Prove the actual generated provider key was refused by the C policy,
        // rather than accepting an unrelated setup failure as custody evidence.
        val key = checkNotNull(store().getKey(alias, null) as? javax.crypto.SecretKey)
        org.junit.Assert.assertThrows(IllegalStateException::class.java) {
            KeystoreWrappingKey.requirePolicy(key)
        }
    }

    @Test fun closingWorkersBoundRepeatedResumeAndAllowExplicitRetry() {
        lateinit var foreground: OwnedExecutor
        val retiring = OwnedExecutor()
        val entered = CountDownLatch(2)
        val release = CountDownLatch(1)
        val timedOut = AtomicBoolean(false)
        onUi {
            val session = MainActivity::class.java.getDeclaredField("session").apply {
                isAccessible = true
            }.get(it) as WalletPlatformSession
            foreground = WalletPlatformSession::class.java.getDeclaredField("work").apply {
                isAccessible = true
            }.get(session) as OwnedExecutor
        }
        fun awaitClosed(owner: OwnedExecutor) {
            val backend = OwnedExecutor::class.java.getDeclaredField("executor").apply {
                isAccessible = true
            }.get(owner) as ThreadPoolExecutor
            assertTrue(backend.awaitTermination(5, TimeUnit.SECONDS))
        }
        try {
            for (owner in listOf(foreground, retiring)) assertTrue(owner.submit {
                entered.countDown()
                if (!release.await(60, TimeUnit.SECONDS)) timedOut.set(true)
            })
            assertTrue(entered.await(5, TimeUnit.SECONDS))
            checkNotNull(scenario).moveToState(Lifecycle.State.STARTED)
            retiring.close()
            assertTrue(foreground.isClosed)
            checkNotNull(scenario).moveToState(Lifecycle.State.RESUMED)
            awaitView(R.id.retry_wallet)
            onUi {
                assertEquals(it.getString(R.string.wallet_busy),
                    it.findViewById<TextView>(R.id.status_message).text.toString())
                assertNull(it.findViewById<View>(R.id.recovery_words))
                assertNull(it.findViewById<View>(R.id.recovery_input))
            }
            // Retry remains bounded while both operations are still finishing.
            click(R.id.retry_wallet)
            awaitView(R.id.retry_wallet)
            release.countDown()
            awaitClosed(foreground)
            awaitClosed(retiring)
            click(R.id.retry_wallet)
            awaitView(R.id.create_wallet)
            assertEquals(CoreStatus.NOT_FOUND, WalletStorage(directory.absolutePath).read().status)
            assertFalse(store().containsAlias(alias))
        } finally {
            release.countDown()
            foreground.close()
            retiring.close()
            awaitClosed(foreground)
            awaitClosed(retiring)
        }
        assertFalse(timedOut.get())
    }

    @Test fun pauseRenderingFailureStillClosesTheForegroundWorker() {
        lateinit var currentSession: WalletPlatformSession
        lateinit var worker: OwnedExecutor
        lateinit var preview: TextView
        val entered = CountDownLatch(1)
        val release = CountDownLatch(1)
        val finished = CountDownLatch(1)
        val timedOut = AtomicBoolean(false)
        val unexpectedRun = AtomicBoolean(false)
        val queuedWords = charArrayOf('a', 'b', 'c') // Public input; no key or wallet setup.
        val problem = IllegalStateException("Synthetic public pause-rendering failure")
        val listener = object : TextWatcher {
            private var fired = false
            override fun beforeTextChanged(text: CharSequence?, start: Int, count: Int, after: Int) {
                if (!fired) { fired = true; throw problem }
            }
            override fun onTextChanged(text: CharSequence?, start: Int, before: Int, count: Int) = Unit
            override fun afterTextChanged(text: Editable?) = Unit
        }
        onUi {
            val sessionField = MainActivity::class.java.getDeclaredField("session").apply { isAccessible = true }
            currentSession = sessionField.get(it) as WalletPlatformSession
            worker = WalletPlatformSession::class.java.getDeclaredField("work").apply {
                isAccessible = true
            }.get(currentSession) as OwnedExecutor
            val screens = MainActivity::class.java.getDeclaredField("screens").apply {
                isAccessible = true
            }.get(it) as WalletScreens
            screens.enterRecovery(false, false, { words -> words.fill('\u0000') }, {})
            val input = it.findViewById<RecoveryInputView>(R.id.recovery_input)
            input.append('a')
            preview = input.getChildAt(0) as TextView
            preview.addTextChangedListener(listener)
        }
        try {
            assertTrue(worker.submit({ finished.countDown() }) {
                entered.countDown()
                timedOut.set(!release.await(30, TimeUnit.SECONDS))
            })
            assertTrue(entered.await(5, TimeUnit.SECONDS))
            assertTrue(worker.submit({ queuedWords.fill('\u0000') }) { unexpectedRun.set(true) })
            onUi {
                val pause = MainActivity::class.java.getDeclaredMethod("onPause").apply { isAccessible = true }
                assertSame(problem, assertThrows(InvocationTargetException::class.java) { pause.invoke(it) }.cause)
                assertTrue("Foreground worker survived failed pause rendering", worker.isClosed)
                assertEquals("Failed background clear left the recovery preview visible", View.INVISIBLE, preview.visibility)
                assertTrue(queuedWords.all { word -> word == '\u0000' })
                assertNull(MainActivity::class.java.getDeclaredField("session").apply {
                    isAccessible = true
                }.get(it))
            }
        } finally {
            // Also recover the old implementation after its expected assertion
            // failure, so the fixture never leaves its bounded worker blocked.
            try { onUi { preview.removeTextChangedListener(listener); currentSession.close() } }
            finally {
                release.countDown()
                queuedWords.fill('\u0000')
            }
            assertTrue(finished.await(5, TimeUnit.SECONDS))
        }
        assertFalse(timedOut.get())
        assertFalse(unexpectedRun.get())
    }

    @Test fun restorePublicFixtureThenAuthenticateAgainToUnlock() {
        click(R.id.restore_wallet)
        authenticate()
        val phrase = "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon about".toCharArray()
        val entropy = ByteArray(16) // Published, unfunded fixture only.
        try {
            enter(phrase)
            val expected = WalletKeys.receivingAddress(entropy, Network.TESTNET).encoded
            assertEquals(expected, address())
            assertFalse(File(directory, ".change.index").exists())
            click(R.id.lock_wallet)
            click(R.id.unlock_wallet)
            authenticate()
            assertEquals(expected, address())
        } finally {
            phrase.fill('\u0000')
            entropy.fill(0)
        }
    }

    @Test fun createRequiresWrittenBackupConfirmationBeforeSaving() {
        click(R.id.create_wallet)
        authenticate()
        awaitView(R.id.recovery_words)
        var backup: CharArray? = null
        var wordsView: RecoveryWordsView? = null
        try {
            onUi {
                val view = checkNotNull(it.findViewById<RecoveryWordsView>(R.id.recovery_words))
                wordsView = view
                backup = CharArray(view.text.length).also { chars -> TextUtils.getChars(view.text, 0, chars.size, chars, 0) }
            }
            assertEquals(CoreStatus.NOT_FOUND, WalletStorage(directory.absolutePath).read().status)
            click(R.id.backup_written)
            onUi { assertEquals(0, checkNotNull(wordsView).text.length) }
            enter(CharArray(0))
            awaitView(R.id.recovery_input)
            assertEquals(CoreStatus.NOT_FOUND, WalletStorage(directory.absolutePath).read().status)
            enter(checkNotNull(backup))
            val saved = address()
            assertEquals(80L, File(directory, ".change.index").length())
            click(R.id.lock_wallet)
            click(R.id.unlock_wallet)
            authenticate()
            assertEquals(saved, address())
        } finally {
            backup?.fill('\u0000')
        }
    }

    @Test fun backgroundingBackupClearsItsViewAndDoesNotSave() {
        click(R.id.create_wallet)
        authenticate()
        awaitView(R.id.recovery_words)
        var wordsView: RecoveryWordsView? = null
        onUi { wordsView = it.findViewById(R.id.recovery_words) }
        checkNotNull(scenario).moveToState(Lifecycle.State.CREATED)
        onUi { assertEquals(0, checkNotNull(wordsView).text.length) }
        assertEquals(CoreStatus.NOT_FOUND, WalletStorage(directory.absolutePath).read().status)
        checkNotNull(scenario).moveToState(Lifecycle.State.RESUMED)
        awaitView(R.id.create_wallet)
    }
}
