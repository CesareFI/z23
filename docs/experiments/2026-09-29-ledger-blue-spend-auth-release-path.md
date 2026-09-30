<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->
# SpendAuth release path on Cortex-M0

## Question

Does the isolated SpendAuth signer skip point multiplication or signing when
the spending scalar is invalid or the host-supplied `rk` disagrees?

## Finding

Previously, canonical-scalar and randomized-key checks returned before
later stages. The revised `blue_sapling_spend_auth_sign` decodes both scalar
inputs, adds them, computes the randomized key, compares it with `rk`, and
runs the isolated signature operation for every non-null, non-overlapping
input set. It masks the entire signature to zero when any stage fails.
The test covers noncanonical `ask` and `ar`, a canceling randomizer, and
an incorrect `rk`; rejected calls leave zero signature and workspace bytes.

The Cortex-M0 disassembly shows no branch on the combined validity value in
the outer SpendAuth function after input storage checks. It contains branches
for public pointer/alias checks and fixed loop bounds. The called signer,
nonce, point encoding, and hash operations have not been shown to have
secret-independent control flow. This change therefore does not establish a
constant-time full signing path or permit enabling it with device keys.

## Reproduction and limits

At 2026-09-29T06:17:18-04:00 (2026-09-29T10:17:18+00:00), the host was an AMD
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

Release passed 62/62 tests in 28.52 seconds; sanitized Debug passed 62/62
in 32.09 seconds. The Cortex-M0 mapped signer still peaks at 1,536 of 2,048
reserved stack bytes, and Cortex-M3 at 1,452 of 2,048. Debug retained
address and undefined-behavior instrumentation; leak detection was disabled
because startup under process tracing is blocked in this environment.

The physical Wallet 0.3.44 image is unchanged. No physical Blue USB, touch,
timing, power, fault, or signing measurement was made. The synthetic-seed
signer remains compile-blocked from the Wallet image.
