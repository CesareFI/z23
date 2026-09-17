// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import android.os.Build
import android.security.keystore.UserNotAuthenticatedException

/** Public provider-failure classification shared by custody fixtures. No
 * provider message, credential, key or alias is copied into fixture output. */
internal fun custodyAuthenticationRefused(error: Throwable): Boolean {
    var current: Throwable? = error
    repeat(8) {
        val cause = current ?: return false
        if (cause is UserNotAuthenticatedException) return true
        if (Build.VERSION.SDK_INT >= 33 && cause is android.security.KeyStoreException &&
            cause.requiresUserAuthentication()) return true
        current = cause.cause
    }
    return false
}
