<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->
# Ledger Blue host signature result isolation

Date: 2026-09-28T18:46:51-04:00 / 2026-09-28T22:46:51Z.

## Question

Can a later callback change an earlier verified signature while the host
signing function still reports success?

## Experiment and result

The two-input payment flow uses a verifier callback that clears the first
caller-owned signature result during verification of the second signature.
Before the change, the function returned success with the first DER length
equal to zero. The Release regression failed at the assertion requiring a
complete first signature. After the change, both signatures remain intact and
the regression passes in Release and sanitized Debug builds.

The function now holds verified replies in a private allocation and copies
them to caller storage only after all inputs pass. On any failure, it clears
caller output, attempts to abort the device review, erases the private
allocation, and frees it. No device image or consensus code changed.

Clang 22.1.6, ISO C23, on AMD Ryzen 7 PRO 8840U measured
`sizeof(blue_payment_verified_signature) == 140` bytes. At the existing
16-input limit, the private allocation is at most 2,240 bytes. Allocation
failure follows the existing abort and cleared-output path. This is host
memory; the Blue's RAM and stack budgets are unchanged.

Commands:

```sh
cmake --build /tmp/z23-blue-standalone-release --target test-blue-payment-review --parallel 2
ctest --test-dir /tmp/z23-blue-standalone-release -R '^blue-payment-review$' --output-on-failure
cmake --build /tmp/z23-blue-standalone-debug --target test-blue-payment-review --parallel 2
ASAN_OPTIONS=detect_leaks=0 ctest --test-dir /tmp/z23-blue-standalone-debug -R '^blue-payment-review$' --output-on-failure
make check-core-seal check-cyclomatic-complexity
ZCL_WINDOWS_ACCEPTANCE_GUARD_SCRATCH=/tmp/z23-blue-sign-isolation-wag make lint-fast
```

The focused test and final full suites passed in both builds: 59/59 Release
and 59/59 sanitized Debug tests. The core seal and complexity cap passed;
`lint-fast` passed all 33 gates. Markdown link and inline path gates passed
across 539 documents.

## Limit

This test establishes the host function's returned-result integrity against
mutation by a later callback. It does not establish real ZCL transaction
readiness, physical Blue behavior, or shielded signing safety. The physical
Wallet 0.3.4 freeze remains unresolved; no new device install was attempted.
