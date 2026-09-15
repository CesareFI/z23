// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet.core

/** Normalized AndroidKeyStore metadata; never populate from untrusted input.
 * The C core owns the acceptance predicate. This cannot authenticate an OS or
 * turn an emulator's virtual key service into physical secure hardware. */
object WrappingPolicy {
    const val TEE = 1
    const val STRONGBOX = 2
    const val AES_GCM_ONLY = 1
    const val GENERATED = 2
    const val AUTH_REQUIRED = 4
    const val HARDWARE_AUTH = 8
    const val ON_BODY = 16
    const val STRONG_BIOMETRIC = 1
    const val DEVICE_CREDENTIAL = 2

    /** UI scheduling hint; the C elapsed-time predicate must still guard
     * actual delivery after sleep or delayed platform callbacks. */
    val authenticationWindowMillis: Long get() = NativeCore.authenticationWindowMillis()
    fun authenticationWindowOpen(startedMillis: Long, nowMillis: Long): Boolean =
        NativeCore.authenticationWindowOpen(startedMillis, nowMillis)

    /** Positive cleanup delay while setup is live; zero refuses an expired or
     * invalid clock. Resample at each use, including after queued delivery. */
    fun setupWindowRemainingMillis(startedMillis: Long, nowMillis: Long): Long =
        NativeCore.setupWindowRemainingMillis(startedMillis, nowMillis)

    fun accepts(keyBits: Int, hardware: Int, flags: Int, authenticationSeconds: Int, methods: Int): Boolean =
        NativeCore.acceptWrappingPolicy(keyBits, hardware, flags, authenticationSeconds, methods)
}
