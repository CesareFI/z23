<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Bounded Sapling note ciphertext authentication

## Intention

Authenticate a fixed 580-byte Sapling note ciphertext and recover its
564-byte plaintext on the Cortex-M0 path without a second ciphertext-sized
MAC buffer. Keep the existing 80-byte outgoing ciphertext behavior.

## Reproduction

```sh
cmake -S apps/zcl-ledger -B build/zcl-ledger-debug \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang \
  -DBLUE_ARM_GCC=/absolute/path/arm-none-eabi-gcc
cmake --build build/zcl-ledger-debug
ctest --test-dir build/zcl-ledger-debug --output-on-failure
```

The host test links Z23's separate C23 ChaCha20-Poly1305 implementation as
the encryption and decryption oracle. It checks a 564-byte patterned
plaintext, a fixed zero-ciphertext vector, ciphertext tampering, tag
tampering, wrong keys, and failure-result clearing. Both ARM emulators run
the fixed vector with a 1536-byte stack ceiling.

## Observation

At 2026-09-28T00:23:07-04:00 (2026-09-28T04:23:07Z), Clang 22.1.6 Debug
with AddressSanitizer and UndefinedBehaviorSanitizer passed all 53 Blue
CTests. GCC 16.1.1 Release passed the focused host test. ARM GNU Toolchain
16.2.0 built ISO C23 for Cortex-M0 and M3; QEMU 11.0.1 passed `NOTEOPEN`
and the existing `OUTOPEN` case on both.
The measured `NOTEOPEN` stack peaks were 1216 bytes on M0 and 1104 bytes
on M3, below the test ceiling. The host CPU was an AMD Ryzen 7 PRO 8840U.
The 33-gate `make lint-fast` run passed.

## Limit

An authenticated note plaintext is not a verified transaction output.
The device still needs a trusted note key derivation, ephemeral public-key
check, note commitment check, recipient policy, and bounded presentation of
value and memo before this can influence approval. This source is not
linked into the installed Blue app. QEMU stack peaks exclude BOLOS and SDK
frames.
