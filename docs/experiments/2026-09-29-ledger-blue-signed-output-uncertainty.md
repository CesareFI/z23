<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Ledger Blue signed-file publication uncertainty

Date: 2026-09-29T08:00:50-04:00 (2026-09-29T12:00:50Z).
Compiler: Clang 22.1.6. CPU: AMD Ryzen 7 PRO 8840U with Radeon 780M Graphics.

## Question

Can the transparent signing command return an error after a complete signed
file becomes visible, contrary to its former failure message that no signed
transaction was retained?

## Result

Yes. The output writer syncs the complete unnamed file before linking it
under the requested name. It then syncs the directory. A failure of that
directory sync triggers a rollback attempt, but rollback itself can fail.
The C23 test wraps Linux `fsync` and `unlinkat` to fail at those two exact
points. `blue_signed_output_commit` returns false while a complete six-byte
test file remains at the requested path; the test reads and compares every
byte before cleanup. Release and AddressSanitizer/UBSan Debug focused tests
pass. The complete C23 Release suite passed 63/63 in 29.00 seconds, and the
complete sanitized Debug suite passed 63/63 in 32.92 seconds with leak
detection disabled for this traced environment. This is a deterministic
host filesystem fault test, not a claim about
physical Blue USB behavior.

The signer now tells the user to inspect the requested output path before
retrying and states that Z23 did not broadcast. The output API documents
that a failed commit never exposes a partial file but can leave a complete
file after a post-link sync and rollback failure. The destination remains
exclusive: a later attempt cannot replace an existing path.
