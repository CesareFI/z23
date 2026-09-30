<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->
# Blue Sapling key derivation storage isolation

## Question

Do the outgoing cipher key and note cipher key derivation helpers preserve
their input fields when the caller supplies an overlapping output buffer?

## Experiment

The outgoing-key test passes an output starting one byte inside the OVK, then
an output equal to the captured value commitment. The note-key test passes
an output starting one byte inside the DH result, then an output equal to
the ephemeral public key. Each rejected call must preserve the entire
overlapping input. Both tests failed before the fix because the derivation
returned success and wrote a key over those inputs.

The C23 helpers now reject overlap with all fixed-size inputs and with the
hasher descriptor before writing. They also reject an output containing the
hash context's base address. Other returned failures erase the key with
volatile byte stores. The caller remains responsible for keeping the full
opaque hash context disjoint from every input and output and erasing it after
use; the hasher interface does not report the context's size.

## Results and limits

At 2026-09-29T04:07:13-04:00 (2026-09-29T08:07:13Z), the host was an
AMD Ryzen 7 PRO 8840U with Clang 22.1.6. The ARM toolchain was
arm-none-eabi-gcc 16.2.0. Release and sanitized Debug CTest each passed
62/62 tests after the fix. The two changed helpers passed ISO C23 Cortex-M3
syntax compilation. The source changes are not linked into the Wallet image;
these tests do not establish device-derived viewing keys, verified recipient
or amount, or physical Blue behavior.
