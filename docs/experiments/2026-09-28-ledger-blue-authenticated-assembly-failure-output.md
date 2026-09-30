<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->
# Ledger Blue authenticated assembly failure output

Date: 2026-09-28T19:42:44-04:00 / 2026-09-28T23:42:44Z.

## Question

Does a rejected authenticated assembly leave a previous signed transaction
and its nonzero length in the caller's output buffer?

## Experiment and result

The host first assembles a valid synthetic transparent transaction, then
reuses the same output buffer and length with an incorrect reviewed-wire
hash. Before the change, authentication returned false while the old signed
length remained nonzero. The Release regression failed at the zero-length
assertion. The corrected authenticated path clears the entire declared output
capacity and sets length to zero after accepted arguments fail, including
hash mismatch, forged signature, verifier refusal, ownership hash mismatch,
callback mutation, allocation failure, or downstream assembly failure.

The existing tests still require rejected overlapping storage to remain
untouched, so an invalid output pointer cannot erase a trusted input. A
declared capacity above the 2 MiB transaction limit is likewise rejected
before wiping; a regression uses a 512-byte buffer with a 2 MiB-plus-one
capacity claim and verifies that the buffer and length stay untouched. The
standalone layout and reviewed-hash assembly primitives retain their
documented behavior; callers preparing a publishable signed transaction
must use authenticated assembly and reject any false return.

This is host-side C23 code. No Ledger Blue image or consensus core changed.

Commands:

```sh
cmake --build /tmp/z23-blue-standalone-release --parallel 4
ctest --test-dir /tmp/z23-blue-standalone-release --output-on-failure --parallel 4
cmake --build /tmp/z23-blue-standalone-debug --parallel 4
ASAN_OPTIONS=detect_leaks=0 ctest --test-dir /tmp/z23-blue-standalone-debug --output-on-failure --parallel 1
make check-core-seal check-cyclomatic-complexity
ZCL_WINDOWS_ACCEPTANCE_GUARD_SCRATCH=/tmp/z23-blue-assemble-cleanup-wag make lint-fast
```

Clang 22.1.6 on AMD Ryzen 7 PRO 8840U built the final source. The full
Release and serial sanitized Debug suites each passed 59/59 tests. The
unchanged core seal, cyclomatic complexity cap, and all 33 fast lint gates
passed. Markdown link and inline path gates passed across 543 documents.

## Limit

Zeroing a rejected output protects the API result from stale signed bytes.
It does not prove that a transaction is funded by confirmed owned UTXOs,
that the Blue display matches every payment fact, or that physical signing
works reliably. The physical Wallet 0.3.4 freeze remains unresolved.
