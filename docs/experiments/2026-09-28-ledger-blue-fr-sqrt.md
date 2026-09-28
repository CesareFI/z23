<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Public Jubjub field square root on Cortex-M0

## Intention

Provide the square-root step needed to decompress public Jubjub point
encodings, using the Blue's measured 32-bit-limb field arithmetic rather
than the host `fr.c` implementation that does not compile for Cortex-M0.

## Reproduction

```sh
cmake -S apps/zcl-ledger -B build/zcl-ledger-debug \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang \
  -DBLUE_ARM_GCC=/absolute/path/arm-none-eabi-gcc
cmake --build build/zcl-ledger-debug
ctest --test-dir build/zcl-ledger-debug --output-on-failure
```

The host test checks zero, one, sixteen deterministic squares,
input/output aliasing, failure-result clearing, and a known nonsquare
against Z23's separate `fr_sqrt` implementation. Because either square
root may be returned, it checks the candidate root by squaring it.

## Observation

On 2026-09-28, Clang 22.1.6 Debug with AddressSanitizer and
UndefinedBehaviorSanitizer passed all 53 Blue CTests on an AMD Ryzen 7
PRO 8840U. GCC 16.1.1 Release passed the focused field test. ARM GNU
Toolchain 16.2.0
built the C23 candidate for Cortex-M0 and M3; QEMU 11.0.1 passed `FRSQRT`
on both. Its measured stack peaks were 1064 bytes on M0 and 988 bytes on
M3, inside the 1536-byte test ceiling.
The 33-gate `make lint-fast` run passed.

## Limit

The algorithm's running time depends on its public input. It must not
process a secret field element. Square root alone does not validate a
Jubjub point, its subgroup, an ephemeral key, or a Sapling note commitment.
QEMU stack peaks exclude BOLOS and SDK frames.
