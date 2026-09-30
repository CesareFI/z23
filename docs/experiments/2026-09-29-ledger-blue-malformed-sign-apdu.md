<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Ledger Blue malformed signing command after approval

Date: 2026-09-29T05:20:49Z (2026-09-29T01:20:49-04:00).
Host CPU: AMD Ryzen 7 PRO 8840U w/ Radeon 780M Graphics.
Compiler: Clang 22.1.6, ISO C23.

## Question

After the owner reviews the fixture payment and taps SIGN ZCL, can a hostile
host send a malformed signing command and then use the still-approved review
to obtain a signature?

## Test and result

The integrated app-loop test reviews the fixture transaction, checks the
displayed output addresses and amounts, fee, branch, lock time, and expiry,
and compares the device-stored ZIP-243 digest with the independent host
calculation. It then taps SIGN ZCL and sends five hostile APDUs in separate
runs: wrong class, nonzero P1, missing input index, extra byte, and unknown
instruction. Each returns its exact error status without calling the signer.
Each failure clears the command buffer beyond its two-byte status, erases the
payment state, cancels the 30-second approval timer, and returns to the
receive screen. A subsequent valid signing APDU fails until a fresh review.
For a two-input fixture, the test also accepts the first signature, sends a
malformed second command, and verifies that the remaining approval and
second digest are erased. A later well-formed second command cannot obtain
the missing signature.

The focused test passed in Release and sanitized Debug builds. The complete
Release suite passed 60/60 in 29.14 seconds and the complete sanitized Debug
suite passed 60/60 in 32.40 seconds with LeakSanitizer disabled for this
runner's ptrace environment; address and undefined-behavior sanitizers
remained enabled. The app-loop
signer captures the requested digest and emits fixed test DER, so this test
proves command and approval lifetime behavior, not ECDSA validity or
physical Blue behavior. The device image is unchanged.
