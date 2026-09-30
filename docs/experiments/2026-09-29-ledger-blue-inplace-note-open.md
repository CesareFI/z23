<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Ledger Blue Sapling ciphertext opening in captured storage

Date: 2026-09-29T08:18:56-04:00 (2026-09-29T12:18:56Z).
Host: AMD Ryzen 7 PRO 8840U. Host compiler: Clang 22.1.6, C23.
ARM emulator compiler: arm-none-eabi-gcc 16.2.0.

## Question and method

Can a bounded C23 decryptor authenticate a captured Sapling ciphertext
before replacing it with plaintext, without a second note plaintext buffer?
The new `blue_sapling_out_open_inplace` and
`blue_sapling_note_open_inplace` accept the existing 80-byte and 580-byte
capture buffers. They verify the tag before decryption, erase the tag on
success, and erase the entire buffer after failed authentication. They
reject key storage overlapping the capture without writing either region.

The host test compares both forms of opening with Z23's independent AEAD
implementation and the committed Sapling transaction fixture. It checks
altered tags, a null key, and overlapping storage. The Cortex-M3 emulator
checks valid and altered outgoing and note ciphertexts. The Cortex-M0
Sapling emulator remains part of the focused test group.

## Results

From the repository root with the existing Release and Debug CMake builds:

```text
ctest --test-dir /tmp/z23-blue-tail-release --output-on-failure -j4
100% tests passed, 0 tests failed out of 63

ASAN_OPTIONS=detect_leaks=0 ctest --test-dir /tmp/z23-blue-tail-debug \
  --output-on-failure -R '^blue-(sapling-ock|m3-qemu|m0-sapling-qemu)$'
100% tests passed, 0 tests failed out of 3

M3 OUTOPEN 0x02a8
M3 NOTEOPEN 0x0460
M3 NOTEINPLACE 0x0458
M3 STACK 0x05ac
M3 PASS
```

The first Debug run failed before the test with LeakSanitizer's
`LeakSanitizer has encountered a fatal error` message in this execution
environment. Repeating with leak detection disabled passed; AddressSanitizer
and UndefinedBehaviorSanitizer remained enabled. The M3 test guards 1536
stack bytes per case. The in-place note case used 0x458 bytes; its buffer
was automatic storage, not a new static allocation.

## Limits

This API reuses an existing capture; it does not by itself reduce the
installed app's RAM or image size. It is not linked into the installed Blue
Wallet. The fixture uses public test keys. Device-secret timing, key
derivation, recipient and amount verification, approval, signing, and
physical Blue behavior remain unproven by these tests.
