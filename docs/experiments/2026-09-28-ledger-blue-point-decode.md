<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Canonical public Jubjub point decoding on Cortex-M0

## Intention

Decode a compressed Sapling public point on the Blue with canonical field
bytes, curve recovery, sign matching, and small-order rejection. Keep all
scratch space caller-owned so the stack can be measured and bounded.

## Reproduction

```sh
cmake -S apps/zcl-ledger -B build/zcl-ledger-debug \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang \
  -DBLUE_ARM_GCC=/absolute/path/arm-none-eabi-gcc
cmake --build build/zcl-ledger-debug
ctest --test-dir build/zcl-ledger-debug --output-on-failure
```

The host test compares 32 nonidentity generator multiples with Z23's
separate `jub_from_bytes` and `jub_to_bytes` path. It rejects the identity,
the order-two point, an invalid sign on the identity, an off-curve encoding,
and an encoding equal to the field modulus.
It checks that failure clears the result and every workspace byte.

## Observation

On 2026-09-28T00:53:40-04:00 (2026-09-28T04:53:40Z), Clang 22.1.6 Debug
with AddressSanitizer and UndefinedBehaviorSanitizer passed all 53 Blue
CTests on an AMD Ryzen 7 PRO 8840U. GCC 16.1.1 Release passed the two
focused field and point tests. ARM GNU Toolchain 16.2.0 compiled the C23
decoder; QEMU 11.0.1 passed `POINT` on Cortex-M0 and M3. The measured stack
peaks were 1232 and 1148 bytes respectively, inside the 1536-byte test
ceiling. The caller-owned workspace is 544 bytes and is erased on return.
The 33-gate `make lint-fast` run passed.

The QEMU test overlays this workspace with later signing-fixture storage,
since its cases execute sequentially. The M0 image occupies 1364 bytes of
`.data` and 2720 bytes of `.bss`, leaving 12 bytes outside its 2048-byte
reserved stack in the 6 KiB test RAM. The M3 image leaves 28 bytes. These
figures describe the test image, not the Blue app's BOLOS memory layout.

## Limit

The decoded point is not a verified Sapling recipient or ephemeral key.
The decoder rejects points of order at most eight but does not itself
perform the protocol's cofactor multiplication, key agreement,
diversifier group hash, or note-commitment check. It is isolated from
device key operations and signing approval.
