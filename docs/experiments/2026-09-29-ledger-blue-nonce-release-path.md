<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->
# Isolated RedJubjub nonce and signature release path

## Question

Do invalid entropy or secret-derived nonce values skip later stages or leave
an old signature in the isolated SpendAuth candidate?

## Finding

For non-null, non-overlapping inputs, the isolated signer now computes the
key point, nonce, nonce point, challenge, and response before releasing a
signature. It masks all 64 result bytes to zero if any stage rejects. The
nonce helper hashes even when the 80-byte entropy input is all zero and
masks its 32-byte output to zero on rejection. Reducing the hash directly
into caller-owned output removed a 32-byte temporary and kept the M0 stack
gate intact. Its API requires output storage disjoint from every input;
the outer signer checks this before calling it. The helper now also enforces
this boundary directly before clearing or writing its output.

The Cortex-M0 disassembly of these two functions has pointer/status and
fixed-loop branches but no branch on the combined validity bit at output
release. Blake2b operation status still controls subsequent hash calls, and
the point, hash, reducer, and encoding callees have not been proven to have
secret-independent control flow. This is not a full-path timing proof.

## Reproduction and limits

At 2026-09-29T06:24:56-04:00 (2026-09-29T10:24:56+00:00), the host was an AMD
Ryzen 7 PRO 8840U with Clang 22.1.6 and arm-none-eabi-gcc 16.2.0.

```sh
cmake --build /tmp/z23-blue-standalone-release -j4
ctest --test-dir /tmp/z23-blue-standalone-release -j1 --output-on-failure
cmake --build /tmp/z23-blue-standalone-debug -j4
ASAN_OPTIONS=detect_leaks=0 LSAN_OPTIONS=detect_leaks=0 \
  ctest --test-dir /tmp/z23-blue-standalone-debug -j1 --output-on-failure
/tmp/z23-arm-toolchain/usr/bin/arm-none-eabi-objdump -d \
  /tmp/z23-blue-standalone-release/blue-m0-sapling-qemu.elf
```

Release passed 62/62 in 28.79 seconds and sanitized Debug passed 62/62 in
32.92 seconds. The focused host test rejects all-zero entropy and
noncanonical or zero secret scalars without leaking a prior signature. The
mapped SpendAuth fixture peaks at 1,536 bytes on M0 and 1,452 bytes on M3
inside 2,048-byte reservations. The M0 emulator image uses 52,008 bytes of
`.text`, 1,364 bytes of `.data`, and 2,728 bytes of `.bss`, including its
fake BOLOS harness. Debug retained address and undefined-behavior
instrumentation; leak detection was disabled because its startup under
process tracing is blocked in this environment.

The physical Wallet 0.3.44 image is unchanged; its synthetic-seed signer
remains compile-blocked. No physical Blue timing, power, fault, touch, USB,
or device-key signing measurement was made.

## Output overlap audit

At 2026-09-29T06:29:47-04:00 (2026-09-29T10:29:47+00:00), an additional
guard began rejecting nonce output that exactly or partially overlaps the
entropy, encoded key point, or transaction digest before the output clear.
The host test checks unchanged caller storage in all four cases. After
splitting the guard to preserve the unchanged complexity cap of 15, Release
passed 62/62 tests in 29.65 seconds and sanitized Debug passed 62/62 in
33.73 seconds; M0/M3 mapped signing still peaks
at 1,536/1,452 bytes. The updated M0 emulator image uses 52,112 bytes of
`.text`, 1,364 bytes of `.data`, and 2,728 bytes of `.bss`, including fake
BOLOS. These are emulator measurements, not physical-device results.
