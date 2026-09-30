<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Ledger Blue authenticated transparent assembly

Date: 2026-09-28T18:40:51Z (2026-09-28T14:40:51-04:00).
Host CPU: AMD Ryzen 7 PRO 8840U w/ Radeon 780M Graphics.
Host compiler: Clang 22.1.6.

## Question

Can a caller produce a signed transaction by passing a syntactically valid
signature record to the wire assembler without verifying the ECDSA signature
and the signing key for each reviewed input?

## Finding and change

The existing layout assembler accepted caller-supplied signature records
whose DER bytes were canonical but not cryptographically authenticated. Its
reviewed-wire variant also checked only the unsigned-wire SHA-256. The fixture
path received records from a separately verified signing function, but that
ordering was a caller convention and the public assembler names obscured it.

The new authenticated entry point checks the reviewed-wire hash, ordered input
index, expected derivation path, expected ZIP-243 digest, canonical low-S DER,
HASH160 of the compressed public key, and ECDSA verification before writing
the signed wire. The synthetic fixture now calls this entry point with the
existing C host crypto callbacks. The lower-level assembly APIs are explicitly
documented as layout primitives without signature authentication.

## Reproduction

From the repository root:

```sh
cmake --build /tmp/z23-blue-standalone-release -j2
ctest --test-dir /tmp/z23-blue-standalone-release -j1 --output-on-failure
cmake --build /tmp/z23-blue-standalone-debug -j2
ASAN_OPTIONS=detect_leaks=0 ctest --test-dir /tmp/z23-blue-standalone-debug -j1 --output-on-failure
ZCL_WINDOWS_ACCEPTANCE_GUARD_SCRATCH=/tmp/z23-blue-auth-wag make lint-fast
make check-core-seal
```

The real secp256k1 signing-flow test assembles one reviewed input through the
new entry point. It rejects a changed signature record, a verifier refusal,
a changed expected key hash, and a changed reviewed-wire hash. Rejection
leaves output bytes and output length untouched. The serial Release and
sanitized Debug suites passed 57/57 tests each. After the final two-input
test edit, the focused review test passed again in both builds. All 33 fast
lint gates passed after splitting the new entry point below the unchanged
cyclomatic cap of 15. Both Markdown gates passed: 523 documents scanned,
with zero new inline-path findings. The core seal matched 554 files and
80 sections. LeakSanitizer was disabled because this container
blocks its ptrace setup; AddressSanitizer and UndefinedBehaviorSanitizer
remained active.

## Limit

The entry point depends on trustworthy HASH160 and secp256k1 verifier
callbacks, and on host preflight supplying the correct prevout-bound digest.
It does not prove that the UTXO exists or that the Blue displayed every
reviewed field. The fixture cannot broadcast. The Wallet image remains
uninstalled after the earlier physical startup freeze; hardware signing is
still unverified.
