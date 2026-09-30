<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->
# RedJubjub response validity on Cortex-M0

## Question

Does the isolated Sapling SpendAuth response change control flow according
to whether its secret nonce or spending scalar is canonical and nonzero?

## Finding

The previous Cortex-M0 build of `blue_redjubjub_response` branched after
decoding the nonce, checking nonce zero, decoding the challenge and secret,
and checking secret zero. Those branches depend on secret-derived values and
skipped multiplication on rejected inputs.

The revised function decodes all three non-null scalars, computes the
response arithmetic, and uses a byte mask to release the response only when
all validity checks pass. Failed calls preserve the caller's output bytes.
The caller initializes output storage before the call because masked release
reads its prior bytes, including on a valid result.
The rebuilt M0 disassembly has null-pointer and fixed-loop/index branches;
it has no branch on the combined scalar-validity bit in this function. This
inspection does not prove constant-time behavior of its callees or of the
complete signer.

## Reproduction and limits

At 2026-09-29T06:08:21-04:00 (2026-09-29T10:08:21+00:00), the host was an AMD
Ryzen 7 PRO 8840U with Clang 22.1.6 and arm-none-eabi-gcc 16.2.0.

```sh
cmake --build /tmp/z23-blue-standalone-release -j4
ctest --test-dir /tmp/z23-blue-standalone-release -j1 --output-on-failure
ASAN_OPTIONS=detect_leaks=0 LSAN_OPTIONS=detect_leaks=0 \
  ctest --test-dir /tmp/z23-blue-standalone-debug -j1 --output-on-failure
/tmp/z23-arm-toolchain/usr/bin/arm-none-eabi-objdump -d \
  /tmp/z23-blue-standalone-release/blue-m0-sapling-qemu.elf
```

The focused RedJubjub equation and Cortex-M0/M3 emulator tests pass. The
mapped-key SpendAuth fixture still peaks at 1,536 bytes on M0 and 1,452 bytes
on M3 within each 2,048-byte stack reservation. The M0 emulator image uses
51,944 bytes of `.text`, 1,364 bytes of `.data`, and 2,728 bytes of `.bss`;
these include the fake BOLOS test harness. The isolated signer remains
compile-blocked from the Wallet. The Wallet 0.3.44 image is unchanged.

Release passed 62/62 in 28.62 seconds, and sanitized Debug passed 62/62 in
32.42 seconds. After initializing the focused test's caller-owned output,
the RedJubjub equation test passed again in both builds. Debug kept address
and undefined-behavior instrumentation enabled; leak detection was disabled
because this environment blocks its startup under process tracing.

No physical Blue timing, power, fault, or secret-key measurement was made.
The full key derivation, point encoding, entropy, and approval paths still
need target side-channel and physical validation before any device-key
signing route can be enabled.
