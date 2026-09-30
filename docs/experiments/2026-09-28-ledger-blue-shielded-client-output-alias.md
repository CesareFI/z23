<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->
# Ledger Blue shielded review output aliasing

Date: 2026-09-28T19:14:18-04:00 / 2026-09-28T23:14:18Z.

## Question

Does a rejected shielded review preserve the caller's transaction when the
review or digest output overlaps its wire? Can overlapping outputs produce a
successful review with corrupted facts?

## Experiment and result

The regression supplies the synthetic Sapling transaction as both the input
wire and the review output. Before the change, the client cleared its review
output before parsing, overwrote the transaction, and failed the byte-for-byte
preservation assertion. A separate case places the digest inside the wire;
another overlaps the review and digest outputs. The final tests require all
three layouts to fail before any USB call and preserve every supplied byte.

The client now checks the three buffer pairs before clearing either output:
review versus wire, digest versus wire, and review versus digest. The check
uses address differences without dereferencing rejected ranges. Valid,
disjoint callers retain the existing zero-output-on-failure behavior. This
change affects only the C23 host client; the read-only Blue app image and
consensus core are unchanged.

Commands:

```sh
cmake --build /tmp/z23-blue-standalone-release --parallel 4
ctest --test-dir /tmp/z23-blue-standalone-release --output-on-failure --parallel 4
cmake --build /tmp/z23-blue-standalone-debug --parallel 4
ASAN_OPTIONS=detect_leaks=0 ctest --test-dir /tmp/z23-blue-standalone-debug --output-on-failure --parallel 4
make check-core-seal check-cyclomatic-complexity
ZCL_WINDOWS_ACCEPTANCE_GUARD_SCRATCH=/tmp/z23-blue-shielded-alias-wag make lint-fast
```

The final Release and sanitized Debug suites each passed 59/59 tests. The
core seal, unchanged cyclomatic complexity cap, and all 33 fast lint gates
passed. Markdown link and inline path gates passed across 541 documents.

## Limit

The shielded client remains read-only. Its replay matches wire structure,
facts, and digest, but it does not verify Sapling proofs, recipients, fee,
ownership, or chain state. No physical-device install was attempted; the
earlier Wallet freeze remains unresolved.
