<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Ledger Blue fixture review and signing boundary

Date: 2026-09-29T02:56:57Z (2026-09-28T22:56:57-04:00).
Host CPU: AMD Ryzen 7 PRO 8840U w/ Radeon 780M Graphics.
Compiler: Clang 22.1.6.

## Question

Does the simulated Blue app sign the digest associated with the amounts and
addresses it showed for a complete transparent transaction, and does it
discard review state after success or a wrong signing request?

## Fixture and method

The integrated app-loop test replays the synthetic one-input, two-output
mainnet fixture and its previous transaction. The host ZIP-243 digest for
input zero is pinned as
`bc49ff238fbb4a7467fd9c4e30ad85a25c80545ea7ccf8c44d27e418b813284b`.
The screen assertions pin the 1 ZCL own output and its full receive address,
the 2 ZCL P2SH output and its full address, the 1 ZCL fee, branch
`0x76B809BB`, lock time 100, and expiry 200. The test reconstructs each
address from the three displayed lines and checks it against the full
address. It checks the final SIGN ZCL and NO SIGN controls before approval.

One pass approves the fixture and requests an out-of-range input index. The
app returns `6985` without calling the signer, clears payment state, returns
to the receive screen, and stops its timer. A second pass approves the same
fixture and signs input zero. A deterministic test signer captures the exact
32-byte digest it receives, and the test compares that digest with the host
fixture digest. The app returns the expected reply framing, clears payment
state, moves to the signed screen, and stops its timer.

The test signer emits a canonical DER shape with fixed `r = 1` and `s = 1`.
That signature is **not valid ECDSA** for the digest and public key. This
experiment proves app-loop review, APDU routing, digest handoff, and cleanup;
it does not prove a cryptographic signature or a physical-device signing
session. The fixed P2SH address is pinned from the current address formatter;
the separate address tests cover version and Base58 behavior. The test does
not establish active-chain UTXO existence or consensus validity.

## Reproduction and result

Build and run the `test-blue-wallet-integrated-loop` target in both Release
and sanitized Debug with the Ledger app's CMake configuration. The focused
test passed in both builds. Before the assertion-only complexity split, the
full Release suite passed 60/60 in 135.91 seconds and the sanitized Debug
suite passed 60/60 in 134.40 seconds. The runner requires
`ASAN_OPTIONS=detect_leaks=0` because LeakSanitizer fails under its ptrace
environment; address and undefined-behavior sanitizers remained enabled.
The first complexity run measured the new signing-boundary test at M=16,
above the unchanged cap of 15. Reply assertions were moved into a separate
helper. The final focused test passed in both builds, and the complexity
gate passed after scanning 63,512 functions. All 33 fast lint gates passed.
The 554-file, 80-section core seal passed, along with Markdown links and
inline paths (552 documents scanned, zero new failures). No device image or
consensus-core source changed in this experiment.
