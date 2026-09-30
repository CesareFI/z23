<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Ledger Blue shielded replay substitution

Date: 2026-09-29T04:59:06Z (2026-09-29T00:59:06-04:00).
Host CPU: AMD Ryzen 7 PRO 8840U w/ Radeon 780M Graphics.
Compiler: Clang 22.1.6, ISO C23.

## Question

Can the host replace transaction bytes during any replay pass after the
first pass and still obtain a completed shielded review digest?

## Test

The shielded APDU test uploads the 4,118-byte Sapling fixture on pass one,
then changes one byte at the fixture midpoint on each of passes two through
six in separate runs. Passes two through five must reject their NEXT command;
pass six must reject FINISH. Each rejection must clear the APDU reply and
erase the full review state. The test passes in the Release and sanitized
Debug builds. The original test covered only substitution on pass two.

The complete C23 Release suite passed 60/60 in 29.97 seconds. The complete
sanitized Debug suite passed 60/60 in 51.79 seconds with LeakSanitizer
disabled for this runner's ptrace environment; address and undefined-behavior
sanitizers remained enabled. The consensus-core seal matched 554 files and
80 sections, the unchanged complexity cap scanned 63,534 functions, and
both Markdown gates passed across 558 documents. The Windows acceptance
guard's self-test required its scratch override under `/tmp` because this
sandbox cannot write the default state directory. With that override, all
33 fast lint gates passed; their 232-second wall time exceeded the 75-second
soft budget under the concurrent host load.

The unchanged production code computes SHA-256 over the complete wire bytes
on each pass and compares it with the first-pass commitment before accepting
the next pass or final digest. The test now exercises that comparison at every
host-controlled replay boundary, including the final one. It does not prove
Sapling signing, ownership of shielded notes, or physical Blue behavior.
