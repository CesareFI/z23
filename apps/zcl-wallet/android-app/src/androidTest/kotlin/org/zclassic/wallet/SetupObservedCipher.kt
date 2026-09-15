// Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
package org.zclassic.wallet

import java.security.AlgorithmParameters
import java.security.Key
import java.security.Provider
import java.security.SecureRandom
import java.security.spec.AlgorithmParameterSpec
import javax.crypto.Cipher
import javax.crypto.CipherSpi

/** Test-only observation around real software GCM with public vectors. The
 * provider is passed to this one Cipher instance, never registered globally
 * or passed through production Keystore acceptance. */
internal class SetupObservedCipher : CipherSpi() {
    private val delegate: Cipher = Cipher.getInstance("AES/GCM/NoPadding")
    var aadCalls = 0
    var finalCalls = 0
    var input: ByteArray? = null // Borrowed public fixture; never copied.
    var afterAad: () -> Unit = {}
    var beforeFinal: () -> Unit = {}
    var afterFinal: () -> Unit = {}

    fun create(): Cipher {
        val provider = object : Provider("PublicSetupFixture", 1.0, "Isolated public GCM observation") {
            init {
                putService(object : Service(this, "Cipher", "AES/GCM/NoPadding",
                    SetupObservedCipher::class.java.name, emptyList(), emptyMap()) {
                    override fun newInstance(parameter: Any?): Any = this@SetupObservedCipher
                })
            }
        }
        return Cipher.getInstance("AES/GCM/NoPadding", provider)
    }

    override fun engineGetParameters(): AlgorithmParameters = delegate.parameters
    override fun engineGetIV(): ByteArray = delegate.iv
    override fun engineGetBlockSize(): Int = delegate.blockSize
    override fun engineGetOutputSize(inputLen: Int): Int = delegate.getOutputSize(inputLen)
    override fun engineSetMode(mode: String) { error("Unexpected test cipher mode change") }
    override fun engineSetPadding(padding: String) { error("Unexpected test cipher padding change") }

    override fun engineInit(opmode: Int, key: Key, random: SecureRandom?) {
        error("Public fixture requires explicit GCM parameters")
    }
    override fun engineInit(opmode: Int, key: Key, params: AlgorithmParameters?, random: SecureRandom?) {
        error("Public fixture requires a GCM parameter spec")
    }
    override fun engineInit(opmode: Int, key: Key, params: AlgorithmParameterSpec?, random: SecureRandom?) {
        delegate.init(opmode, key, params, random)
    }

    override fun engineUpdateAAD(src: ByteArray, offset: Int, len: Int) {
        delegate.updateAAD(src, offset, len)
        aadCalls++
        afterAad()
    }
    override fun engineDoFinal(bytes: ByteArray?, offset: Int, len: Int): ByteArray {
        input = checkNotNull(bytes)
        finalCalls++
        beforeFinal()
        val output = delegate.doFinal(bytes, offset, len)
        afterFinal()
        return output
    }
    override fun engineDoFinal(bytes: ByteArray?, offset: Int, len: Int, output: ByteArray, outputOffset: Int): Int =
        error("Unexpected test cipher output overload")
    override fun engineUpdate(bytes: ByteArray?, offset: Int, len: Int): ByteArray =
        error("Unexpected test cipher streaming input")
    override fun engineUpdate(bytes: ByteArray?, offset: Int, len: Int, output: ByteArray, outputOffset: Int): Int =
        error("Unexpected test cipher streaming output")
}
