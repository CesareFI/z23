# Pinned C cryptographic dependencies

Mbed TLS **3.6.7** (LTS) comes from the upstream release asset:

`https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-3.6.7/mbedtls-3.6.7.tar.bz2`

Archive SHA-256:
`a7e8bcbec0e6f761b4af24f25677626b35f762f68eef79c08677a363212d11f6`.

Source resides at repository `vendor/android-mbedtls/`, the established
third-party source boundary. App-authored code remains subject to the normal
complexity gate. No upstream cryptographic code or self-test is rewritten to
satisfy an application complexity threshold.

The archive digest was checked against the release API's SHA-256 metadata on
2026-09-11. This identifies bytes obtained over HTTPS; it is not an independent
publisher-signature verification or proof of safety. The copied header closure
and four source files are unmodified. `common.h` and `alignment.h` supply their
private header closure. Unused headers are not included. The upstream
Apache-2.0 license is retained.

`zcl_mbedtls_config.h` enables only SHA-256, SHA-512, RIPEMD-160 and primitive
self-tests. There is no TLS implementation linked at this stage. These are
required Zclassic primitives; RIPEMD-160 is not selected for a new protocol.

Authored wrappers apply explicit bounds, check provider errors and return
typed failures. Upstream code is separately identified and must remain covered
by sanitizer/known-answer validation when used. No third-party code is asserted
memory-safe merely because it has been vendored.

## libsecp256k1

The C provider is upstream **0.8.0**, copied unchanged beneath repository
`vendor/android-secp256k1/`. Release archive:
`https://github.com/bitcoin-core/secp256k1/releases/download/v0.8.0/libsecp256k1-0.8.0.tar.gz`.
SHA-256: `dd685546f9e717b9adde329acd5a4cd8083d40f710fdb0a603ee5a83f908132b`.
Selected release files retain the MIT license and a per-file SHA-256 manifest.
No source arithmetic, precomputed table or provider test is rewritten.

The detached release signature verified on 2026-09-11 with full fingerprint
`6A8F9C266528E25AEB1D7731C2371D91CB716EA7`, listed for Sebastian Falbesoner in the
release's SECURITY.md. The public key was retrieved from keys.openpgp.org by
that fingerprint into a temporary public-only verification keyring. This binds
the archive to that listed key; it does not establish personal identity through
an independently trusted certification chain or prove the code safe.

Android links only the core ECDSA/key primitives. Unused ECDH/recovery/Schnorr/
MuSig/ElligatorSwift/silent-payment modules and assembly are disabled. Verification
window 8 and the 22 KiB generator table bound mobile data size. Provider tests
run separately with the same arithmetic/table configuration. Optional modules
remaining as inert vendor source do not create app features or messaging keys.
