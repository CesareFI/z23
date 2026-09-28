<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Transparent review-to-signature host simulation

Local time: 2026-09-27T10:19:05-04:00

UTC: 2026-09-27T14:19:05Z

## Result

A C23 host test now generates a secp256k1 key, places its HASH160 in a
synthetic previous output and a spending transaction's own output, and
binds the previous transaction to the spending outpoint by SHA-256d. The
same test prepares two output pages, simulates both touchscreen CONTINUE
actions, replays the exact transaction to the Blue APDU model, uploads the
previous transaction, and compares the device-model ZIP-243 digest and
calculated fee with independent host values.

The first run uses the current read-only final confirmation. A signing
command then fails with status `6985`, returns no bytes, and never calls the
signer. The second run simulates a separate future signing-approval touch.
Its one-use signing command produces a low-S ECDSA signature for the
transaction's input-specific digest. OpenSSL independently verifies the
signature against the generated key. The approval latch is consumed after
one signature.

A two-input signing fixture simulates a USB reset or app restart after the
first signature. Clearing the review state makes the second signing command
return `6985`; the signer is not called again, and the reply is zeroed. At
2026-09-27T10:46:30-04:00 (2026-09-27T14:46:30Z), the focused test passed
1/1 under Clang 22.1.6 Debug/ASan/UBSan and 1/1 under GCC 16.1.1 Release
on an AMD Ryzen 7 PRO 8840U.

The focused payment-review test passed 1/1 under Clang 22.1.6
Debug/ASan/UBSan and GCC 16.1.1 Release on an AMD Ryzen 7 PRO 8840U. The
repository cyclomatic-complexity ratchet passed 60,794 functions at cap 15.

## Limit

The positive signing approval is a direct host simulation of a future
touchscreen callback. Wallet 0.2.17 has no signing-approval screen or routed
signing APDU, remains uninstalled, and cannot sign. The test does not prove
chain inclusion, UTXO status, network synchronization, actual Blue key
derivation, physical touch delivery, or USB timing. OpenSSL is used by the
test signer and verifier; the standalone read-only reviewer still links
without it.
