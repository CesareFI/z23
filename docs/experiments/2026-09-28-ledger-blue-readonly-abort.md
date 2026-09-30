<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Ledger Blue read-only review cleanup

Local time: 2026-09-28T20:16:21-04:00. UTC: 2026-09-29T00:16:21Z.
Host compiler: Clang 22.1.6. CPU: AMD Ryzen 7 PRO 8840U w/ Radeon 780M Graphics.

## Question

Does the read-only mainnet review command release the Blue's reviewed
transaction and signing state before reporting success or checking the local
node again?

## Finding

The previous host command left the Blue on its final payment screen and asked
the operator to tap NO SIGN. The read-only command now sends the payment-abort
APDU immediately after review, requires an exact `9000` acknowledgement, and
only then repeats the node tip and UTXO checks. If the USB exchange or
acknowledgement fails, it reports the device state as uncertain and instructs
the operator to restart the app before another transaction.

The host APDU simulator completes a bound review, sends the same abort
command, and verifies that the fee and approval are erased. Both final touch
paths then fail, and a signing APDU is denied without invoking the signer.
Separate callback cases reject a short reply, a device error, extra bytes,
and a transport failure.

## Reproduction

```sh
cmake --build /tmp/z23-blue-standalone-release --parallel 4
ctest --test-dir /tmp/z23-blue-standalone-release --output-on-failure --parallel 1
cmake --build /tmp/z23-blue-standalone-debug --parallel 4
ASAN_OPTIONS=detect_leaks=0 ctest --test-dir /tmp/z23-blue-standalone-debug --output-on-failure --parallel 1
make check-core-seal check-cyclomatic-complexity
ZCL_WINDOWS_ACCEPTANCE_GUARD_SCRATCH=/tmp/z23-blue-readonly-abort-wag make lint-fast
make check-markdown-links check-doc-inline-paths
```

All 59 Blue CTest cases passed in Release and sanitized Debug, including the
focused `blue-payment-review` and `blue-wallet-review-cli` cases. The
consensus-core seal matched 554 files and 80 sections. The complexity cap of
15 passed without new pins.

## Limit

The callback and device-state tests run on a host simulator. The abort
acknowledgement checks the app protocol response; it cannot prove physical
RAM erasure on an untested Blue. This change does not add live signing,
independent peer synchronization, or shielded transaction authorization.
