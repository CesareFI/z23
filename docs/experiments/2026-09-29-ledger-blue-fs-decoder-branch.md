<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->
# Ledger Blue scalar decoding on Cortex-M0

## Question

Does the isolated Sapling signer decode canonical secret scalars without
branching on their values in the compiled Cortex-M0 path?

## Finding

The prior `blue_fs_from_bytes_canonical` used four 64-bit comparisons and
branched to erase a noncanonical value. `arm-none-eabi-objdump` of the M0
emulator image showed conditional branches during the comparison, including
branches on equality of secret-derived limbs. The signer candidate remains
compile-blocked for physical device keys.

The decoder now subtracts each of eight 32-bit limbs using a 64-bit unsigned
intermediate and carries the borrow through the fixed loop. A borrow mask
zeros invalid output limbs without a value-dependent branch. Source/result
overlap is rejected before a write, preserving the caller's bytes. The new
host test compares 4,096 arbitrary encodings with the independent scalar
decoder and checks exact and partial source/result aliases. Existing scalar
arithmetic and mapped SpendAuth vectors pass on host and Cortex-M0/M3.

## Measurement and limits

At 2026-09-29T05:58:36-04:00 (2026-09-29T09:58:36+00:00), the host was an AMD
Ryzen 7 PRO 8840U with Clang 22.1.6 and arm-none-eabi-gcc 16.2.0. The rebuilt
M0 disassembly has pointer-validity and fixed-loop branches. Its canonicality
borrow uses `subs`, `sbcs`, and a mask; it has no conditional branch on the
decoded limb values in this function. This is an inspection of one compiled
function, not a timing or side-channel proof for the full signing path.

```sh
cmake --build /tmp/z23-blue-standalone-release -j4
ctest --test-dir /tmp/z23-blue-standalone-release -j1 --output-on-failure
ASAN_OPTIONS=detect_leaks=0 LSAN_OPTIONS=detect_leaks=0 \
  ctest --test-dir /tmp/z23-blue-standalone-debug -j1 --output-on-failure
/tmp/z23-arm-toolchain/usr/bin/arm-none-eabi-objdump -d \
  /tmp/z23-blue-standalone-release/blue-m0-sapling-qemu.elf
```

The Cortex-M3 mapped-key SpendAuth test uses 1,452 bytes of the 2,048-byte
fixture stack. Cortex-M0 uses 1,536 bytes, meeting its 512-byte free-stack
threshold exactly. The emulator images include fake BOLOS calls and public
synthetic seed material. They do not establish Blue target timing, cache,
power, fault, or secret-key behavior. The device-derived signer remains
compile-blocked and is absent from the Wallet image.

The Release suite passed 62/62 in 29.51 seconds. The sanitized Debug suite
passed 62/62 in 77.32 seconds with AddressSanitizer and UndefinedBehaviorSanitizer
enabled. LeakSanitizer was disabled because this environment blocks its
startup under process tracing. The pinned Wallet 0.3.44 image did not change.
