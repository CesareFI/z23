<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Ledger Blue low-memory scalar input isolation

## Finding

The isolated C23 Jubjub multiplier reads a scalar bit on each of 256
iterations while writing the output point. When the scalar points into that
output, the first point write changes later scalar bytes. The prior function
accepted this overlap. A test with scalar storage inside the result point
failed before the fix because multiplication returned success.

## Change and evidence

The multiplier now rejects result/scalar overlap and partial result/point
overlap before writing. Exact result/point alias remains supported through
the existing copied-input path. The regression checks that a rejected
overlap leaves the entire result unchanged; existing point and signing
vectors still pass. Release and sanitized Debug each passed the low-memory
multiplier and RedJubjub equation tests, plus Cortex-M3 and Cortex-M0
emulator tests. The ARM tests exercise the same rejection and unchanged
output on each target: `SCALAR ALIAS <=0x0200` bytes of modeled stack.
The full M3 and M0 emulator suites reported respective peak modeled stacks
of `0x05ac` and `0x0600` bytes. The serial Release and sanitized Debug
host suites each passed 62/62 tests; all 33 fast lint gates, the unchanged
consensus-core seal, and the Markdown documentation gates passed.
These observations were recorded on 2026-09-29T09:26:51Z with Clang 22.1.6,
ARM GCC 16.2.0, and an AMD Ryzen 7 PRO 8840U host.

This helper remains isolated from device-derived keys and a payment
approval command. The device-key signer is still behind its compile-time
barrier pending target side-channel validation. Emulator success does not
establish physical timing or safe device-key use.
