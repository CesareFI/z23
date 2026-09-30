<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Ledger Blue Sapling ephemeral key verification

Date: 2026-09-28T09:48:27Z (2026-09-28T05:48:27-04:00).
Host CPU: AMD Ryzen 7 PRO 8840U w/ Radeon 780M Graphics.
Host compiler: Clang 22.1.6. ARM compiler: arm-none-eabi-gcc 16.2.0.

## Question

Can the C23 Blue arithmetic path reproduce a transaction output's Sapling
ephemeral key from authenticated outgoing plaintext while retaining a usable
stack margin on Cortex M3 and M0?

## Method

`blue_sapling_epk_matches` derives the diversifier base with personalized
BLAKE2s and Jubjub decoding, clears the cofactor, multiplies by the outgoing
ephemeral secret, and compares the compressed result with the transaction's
captured `epk`. It reuses Z23's C23 BLAKE2s implementation and a caller owned
544-byte decode workspace. The host test uses the consensus-accepted simnet
one-spend, one-output fixture after ZIP-243 replay, outgoing ciphertext
opening, and note ciphertext opening. It also changes the diversifier,
ephemeral secret, and `epk`, and checks rejected workspace overlap and failure
wiping. Cortex M3 and M0 QEMU run the fixed fixture vector and a changed
`epk` vector with stack guards.

Commands from the repository root:

```sh
cmake -S apps/zcl-ledger -B /tmp/z23-blue-standalone-release -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/z23-blue-standalone-release -j2
ctest --test-dir /tmp/z23-blue-standalone-release -j1 --output-on-failure
```

## Result

The fixture `epk` matched on host, Cortex M3, and Cortex M0. Changed inputs
were rejected. The isolated EPK case used 1,380 bytes of the 2,048-byte M3
test stack and 1,496 bytes of the 2,048-byte M0 test stack. Both remain below
the emulator's 1,536-byte case limit, which reserves 512 bytes of headroom.
Release CTest passed 55/55; sanitized Debug CTest passed 55/55 with
`ASAN_OPTIONS=detect_leaks=0` because the container blocks LeakSanitizer's
ptrace setup. AddressSanitizer and UndefinedBehaviorSanitizer remained active.
`lint-fast` passed 33 gates; the consensus core seal passed 554 files and 80
sections; Markdown link and inline path checks passed 500 documents; the
cyclomatic complexity ratchet passed at cap 15.
The first implementation used 1,924 bytes on M3 and 2,040 bytes on M0 and
failed that limit. Moving the decode workspace to caller owned RAM corrected
the stack failure without increasing the QEMU image's global memory footprint.

This verifies only the ephemeral key relationship for a public test fixture.
It does not verify the note commitment, recipient address, viewing key
derivation, memo binding, installed BOLOS image, or physical Blue behavior.
No memo is displayed or authorized for signing by this code.
