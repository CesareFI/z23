<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->
# Ledger Blue host signing output after abort

Date: 2026-09-28T20:03:17-04:00 / 2026-09-29T00:03:17Z.

## Question

Can a failed host signing attempt return nonzero signature records because
its final abort exchange callback writes to caller-owned output?

## Experiment and result

The simulated Blue exchange writes nonzero bytes into the result record when
it receives the abort APDU. Before the change, an identity mismatch made
host signing return false with those bytes still present. The Release
regression failed at the zero-output assertion. A second case supplies a
missing expected-hash array and reaches the invalid-argument abort path.

The host signer now performs the abort callback first, then clears all
bounded, disjoint caller result slots. Rejected output/input overlap remains
untouched, preserving trusted inputs. A small helper keeps both abort paths
under the unchanged cyclomatic complexity cap.

The change affects C23 host code only. The Blue app image and consensus
core are unchanged.

Commands:

```sh
cmake --build /tmp/z23-blue-standalone-release --parallel 4
ctest --test-dir /tmp/z23-blue-standalone-release --output-on-failure --parallel 1
cmake --build /tmp/z23-blue-standalone-debug --parallel 4
ASAN_OPTIONS=detect_leaks=0 ctest --test-dir /tmp/z23-blue-standalone-debug --output-on-failure --parallel 1
make check-core-seal check-cyclomatic-complexity
ZCL_WINDOWS_ACCEPTANCE_GUARD_SCRATCH=/tmp/z23-blue-sign-abort-output-wag make lint-fast
```

Clang 22.1.6 on AMD Ryzen 7 PRO 8840U built the final source. The focused
`blue-payment-review` test passed in Release and sanitized Debug. An initial
parallel Release suite had three unchanged QEMU tests time out under shared
load. The final serial Release and sanitized Debug suites each passed 59/59
tests in 40.79 and 53.10 seconds. The core seal, complexity cap, and all 33
fast lint gates passed. Markdown link and inline path gates passed across
544 documents.

## Limit

This establishes a zero-result failure contract for synchronous abort
callbacks and disjoint output storage. It does not prove physical signing,
chain UTXO status, shielded spending, or broadcast safety. The physical
Wallet 0.3.4 freeze remains unresolved.
