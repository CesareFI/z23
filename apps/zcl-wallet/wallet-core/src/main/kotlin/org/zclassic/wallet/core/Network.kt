// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet.core

/** Network identity must come from configuration, never inferred from an address. */
enum class Network(internal val nativeId: Int) {
    MAINNET(0),
    TESTNET(1),
}
