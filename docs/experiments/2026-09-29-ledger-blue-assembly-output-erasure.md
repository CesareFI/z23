<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Ledger Blue host assembly output erasure

Date: 2026-09-29T07:18:46-04:00 (2026-09-29T11:18:46Z).

## Question

Can a failed reviewed-wire check leave a previous signed transaction length
visible, or can a successful assembly leave bytes from an earlier
transaction beyond the new wire length?

## Result

The reviewed assembly entry point previously returned false on a SHA-256
mismatch while preserving a caller's previous output length. It now checks
full-capacity disjointness before writing, clears accepted output and length,
then checks the reviewed hash and assembles scripts. Invalid overlap or an
oversized capacity leaves caller storage untouched. The authenticated
assembly path also clears accepted output immediately before writing the
verified wire, so a successful result has no stale bytes after its length.

The C23 test poisons the output before success, changes an output amount
after review, and checks that a mismatched hash returns zero length and an
erased buffer. It also checks that an output alias with the transaction and
a capacity above the 2 MiB limit reject without touching caller storage.
For authenticated assembly, the same fixture checks zeroed unused capacity
after signature, path, digest, public-key hash, and reviewed-wire checks.
This is host behavior; it is not a physical Ledger Blue signing result.

On Clang 22.1.6 and an AMD Ryzen 7 PRO 8840U with Radeon 780M Graphics,
Release passed 62/62 tests in 30.18 seconds. Address- and
undefined-behavior-sanitized Debug passed 62/62 in 34.97 seconds with leak
detection disabled for this traced environment. The 33 fast lint gates,
554-file/80-section consensus-core seal, and cyclomatic cap 15 passed.
No device image or consensus-core source changed.
