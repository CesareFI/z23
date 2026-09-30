<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Ledger Blue two-input payment loop

Date: 2026-09-29T03:57:24Z (2026-09-28T23:57:24-04:00).
Host CPU: AMD Ryzen 7 PRO 8840U w/ Radeon 780M Graphics.
Compiler: Clang 22.1.6, ISO C23.

## Question

Can the simulated Wallet app review a streamed transaction with two owned
transparent inputs and two outputs, bind both previous transactions, and
consume exactly the approved input digests in order?

## Experiment

The integrated app test extends the existing synthetic fixture with a second
distinct previous transaction and input. The first previous output is four
ZCL, the second is one ZCL, and the outputs total three ZCL. Host preflight
computes a two ZCL fee and distinct ZIP-243 digests. The test streams the
unsigned transaction and both previous wires through the actual app APDU
loop, taps CONTINUE for each displayed output, checks the fee and both bound
input records, and taps SIGN ZCL. It requests input zero and then input one,
checks each captured digest and reply index, and checks that review state is
erased on completion. A separate run requests input one first and requires
`6985`, no signer call, and erased review state.

The app-loop test signer emits a fixed DER-shaped fixture. It proves APDU,
display, digest binding, order, and state cleanup behavior, not ECDSA
validity. The separate signing tests cover ECDSA verification. Neither
synthetic previous wire has established chain provenance, so this experiment
does not prove a spendable payment or safe broadcast.

## Results

The focused integrated test passed in the C23 Release build. The full Release
suite passed 60/60 tests in 83.40 seconds. The sanitized Debug suite passed
60/60 tests in 112.01 seconds with LeakSanitizer disabled for this runner's
ptrace environment; address and undefined-behavior sanitizers remained
enabled. No device firmware changed, and no physical Blue run was made. No
consensus-core source changed.
