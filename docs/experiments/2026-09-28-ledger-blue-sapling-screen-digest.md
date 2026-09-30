<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Ledger Blue accepted Sapling fixture screen and digest

Date: 2026-09-29T03:19:07Z (2026-09-28T23:19:07-04:00).
Host CPU: AMD Ryzen 7 PRO 8840U w/ Radeon 780M Graphics.
Compiler: Clang 22.1.6.

## Question

Does the read-only shielded review show the exact transaction facts and
ZIP-243 digest computed for the consensus-accepted simnet Sapling fixture?

## Method and result

The existing client/device app-loop test uploads all 1,425 fixture bytes in
six passes and compares the resulting digest with the pinned value
`d4967a8269007709fd063a592f7359b864fa390c76f4609dc9f9b96069c27c8b`.
This experiment pins all six summary lines and all six digest-page lines
after that replay. The summary states zero public inputs and outputs, zero
public output value, one shielded spend and output, zero Sprout JoinSplits,
unknown fee, and no signing. The displayed prefix and four digest lines
match the pinned digest. The test also checks the transition to the digest
page before toggling large text and dark mode.

For a sensitivity check, one displayed digest-prefix nibble was changed in
the formatter in a temporary build. The new test failed at its exact screen
comparison. The mutation was removed, and the focused Release and sanitized
Debug tests passed again. The mutation never entered the commit.

The fixture is a deterministic simnet transaction accepted by the local
consensus fixture, not a mainnet payment. The review app does not decrypt
the recipient or amount, establish a fee, check the active chain, or sign.
The screen labels the unknown fee, hidden shielded details, and no signing;
it does not display chain state. This experiment protects the displayed
read-only facts against regressions; it does not demonstrate shielded
payment approval or physical-device behavior.

The full Release suite passed 60/60 in 29.01 seconds, and sanitized Debug
passed 60/60 in 55.33 seconds with LeakSanitizer disabled for this runner's
ptrace environment. Address and undefined-behavior sanitizers remained
enabled. No device image or consensus-core source changed. All 33 fast lint
gates passed.
The complexity gate scanned 63,513 functions at the unchanged cap of 15.
The 554-file, 80-section core seal and both Markdown gates passed; the
inline-path gate scanned 553 documents with zero new failures.

## Next verification

Before shielded signing can be exposed, a device-side fixture must bind the
actual recipient, amount, fee, selected branch, and spend authority to the
transaction digest and to distinct review pages. The current read-only
screen does not establish those facts. A physical Blue run must then verify
the touch flow and failure cleanup against the image built from the final
source.
