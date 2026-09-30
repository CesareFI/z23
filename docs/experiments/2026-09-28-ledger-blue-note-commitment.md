<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Ledger Blue Sapling note commitment

Date: 2026-09-28T10:09:14Z (2026-09-28T06:09:14-04:00).
Host CPU: AMD Ryzen 7 PRO 8840U w/ Radeon 780M Graphics.
Host compiler: Clang 22.1.6. ARM compiler: arm-none-eabi-gcc 16.2.0.

## Question

Can the Blue's C23 arithmetic recompute a Sapling note commitment from
decrypted note contents and match a captured transaction output within the
emulator's 6 KiB SRAM and 2 KiB stack model?

## Method

The isolated verifier uses personalized BLAKE2s GroupHash to derive the
diversifier base, four Pedersen segment generators, and the note randomness
generator. It processes the 582 commitment-preimage bits in 3-bit windows,
adding signed Jubjub point multiples directly instead of allocating the
host core's large precomputed table. It adds the note randomness point and
compares the affine x-coordinate with the captured `cm`. The C23 workspace is
statically capped at 1,024 bytes.
Every window executes the same point operations and selects its magnitude and
sign with masks. Target timing and power behavior remain unmeasured.

The host test opens the consensus-accepted simnet one-spend, one-output
transaction fixture after six-pass ZIP-243 replay. The fixture file SHA-256
is `3c3a1845b53161dbbcfc8664d336680ed5e45cb57d3d7ebb6bd4df6c44f79868`.
The test rejects changed `cm`, value, `pk_d`, diversifier, randomness,
noncanonical randomness, malformed lead byte, and overlapping buffers.
Changing only the memo plaintext leaves the note commitment unchanged. The
separate authenticated ciphertext check must also pass before memo display.
The Cortex M3 and M0 cases check the same captured commitment and changed
value and `cm`, then inspect guard bytes below the 2 KiB stack.

Commands from the repository root:

```sh
cmake -S apps/zcl-ledger -B /tmp/z23-blue-standalone-release -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/z23-blue-standalone-release -j2
ctest --test-dir /tmp/z23-blue-standalone-release -j1 --output-on-failure
cmake -S apps/zcl-ledger -B /tmp/z23-blue-standalone-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/z23-blue-standalone-debug -j2
ASAN_OPTIONS=detect_leaks=0 ctest --test-dir /tmp/z23-blue-standalone-debug -j1 --output-on-failure
```

## Result and integration limit

Release and sanitized Debug passed 57/57 Blue cases. AddressSanitizer and
UndefinedBehaviorSanitizer were active; LeakSanitizer was disabled because
the container blocks its ptrace setup. The commitment case used 1,372 bytes
of stack on Cortex M3 and 1,496 bytes on Cortex M0. The emulator's 1,536-byte
case limit therefore retained at least 552 bytes of the 2 KiB stack. The
standalone M3 image used 13,920 bytes of `.text`, 1,360 bytes of `.data`, and
2,108 bytes of `.bss`; the M0 image used 14,672, 1,364, and 2,120 bytes,
respectively. Each includes test and runtime code, not the Wallet UI.

The existing uninstalled Wallet image has 5,120 bytes of `.bss`, including
its reserved 2,048-byte stack, within the 6,144-byte Blue app SRAM. Its
1,024-byte apparent margin cannot hold the commitment workspace plus a
564-byte decrypted note. The Wallet must reuse mutually exclusive review
memory and stream or re-request authenticated ciphertext before connecting
this verifier to the touchscreen. The current experiment uses public test
data. It does not derive a device viewing key, establish recipient ownership,
display a memo, authorize Sapling signing, or prove behavior on a physical
Blue.
