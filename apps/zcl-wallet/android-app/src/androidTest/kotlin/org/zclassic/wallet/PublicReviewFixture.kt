// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import androidx.test.platform.app.InstrumentationRegistry
import java.security.MessageDigest
import org.zclassic.wallet.core.Network
import org.zclassic.wallet.core.TransparentAddress
import org.zclassic.wallet.core.UnsignedReview
import org.zclassic.wallet.core.Zatoshi

/** Instrumentation-only profile. All sources/proofs/signatures are synthetic;
 * this factory never touches wallet state, keys, endpoints or real funds. */
internal class PublicReviewFixture {
    private val full = when (val profile = InstrumentationRegistry.getArguments().getString("reviewSourceProfile")) {
        null, "narrow" -> false
        "full" -> true
        else -> error("Unknown public review source profile: $profile")
    }
    val processProfile: String get() = if (full) "review-full" else "review"
    private fun fixture(name: String): ByteArray = InstrumentationRegistry.getInstrumentation().context.assets
        .open("${if (full) "full-review" else "review"}/$name").use { it.readBytes() }

    /** Independent Java SHA256d of the committed expected current wire. */
    val transactionId: String by lazy {
        val sha = MessageDigest.getInstance("SHA-256")
        sha.digest(sha.digest(fixture("draft"))).reversedArray().joinToString("") { "%02x".format(it.toInt() and 255) }
    }

    fun open(network: Network = Network.MAINNET, clock: () -> Long): UnsignedReview =
        if (full) prepare(network, clock) else UnsignedReview.open(fixture("draft"),
            arrayOf(fixture("previous0"), fixture("previous1")), network, Zatoshi.of(500), clock)

    fun prepare(network: Network = Network.MAINNET, clock: () -> Long): UnsignedReview {
        val funding = List(2) { UnsignedReview.Funding(fixture("previous$it"), it.toLong(), 0xffff_ffffL) }
        val outputs = listOf(
            UnsignedReview.Output(TransparentAddress.fromPublicKeyHash(ByteArray(20) { 0x55 }, network), Zatoshi.of(9000)),
            UnsignedReview.Output(TransparentAddress.fromScriptHash(ByteArray(20) { 0x66 }, network), Zatoshi.of(1500)))
        return if (full) UnsignedReview.prepareFullSources(funding, outputs, network, 0, 0, Zatoshi.of(500), clock)
            else UnsignedReview.prepare(funding, outputs, network, 0, 0, Zatoshi.of(500), clock)
    }
}
