<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Canonical Jubjub base-field bytes on Cortex-M0

## Intention

Decode untrusted Jubjub field bytes into the Blue's 32-bit-limb Montgomery
representation without importing the host `fr.c` implementation. Reuse the
same conversion for point encoding. This is a prerequisite for checking
Sapling public points from transaction wire bytes.

## Reproduction

```sh
cmake -S apps/zcl-ledger -B build/zcl-ledger-debug \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang \
  -DBLUE_ARM_GCC=/absolute/path/arm-none-eabi-gcc
cmake --build build/zcl-ledger-debug
ctest --test-dir build/zcl-ledger-debug --output-on-failure
```

## Observation

On 2026-09-28T00:37:38-04:00 (2026-09-28T04:37:38Z), Clang 22.1.6
Debug with AddressSanitizer and UndefinedBehaviorSanitizer passed 53/53
Blue CTests on an AMD Ryzen 7 PRO 8840U. GCC 16.1.1 Release passed the
focused field test. The host test compares 4,096 deterministic valid
encodings with the independent core field implementation and checks zero,
one, modulus minus one, modulus, and modulus plus one. The Blue decoder
rejects encodings at or above the modulus and clears its result.
ARM GNU Toolchain 16.2.0 compiled the C23 candidate for Cortex-M0 and M3;
QEMU 11.0.1 passed `FRCODEC` on both. Its measured stack peaks were
624 bytes on M0 and 532 bytes on M3, below the 1536-byte test ceiling.
The 33-gate `make lint-fast` run passed.

The host `core/modules/sapling/src/fr.c` currently returns true for a
32-byte encoding equal to the field modulus: its comparison rejects greater
limbs but has no equality rejection. This was observed when the Blue test
initially used `fr_from_bytes` as an oracle for the exact-modulus rejection.
The host behavior requires a separate consensus-parity assessment before
changing core validation; this experiment does not change it.

## Limit

Canonical field decoding alone does not validate a Jubjub point. The Blue
still needs curve decompression, subgroup checks, diversifier group hash,
ephemeral-key agreement, and note-commitment verification. QEMU stack
measurements exclude BOLOS and SDK frames.
