<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Sapling outgoing AEAD on Cortex-M0

## Intention

Authenticate and decrypt the selected Sapling outgoing ciphertext on a
small ARM target after six-pass transaction replay. The outgoing plaintext
contains the recipient key and ephemeral secret needed for subsequent note
verification. No payment approval follows from this result alone.

## Reproduction

```sh
cmake -S apps/zcl-ledger -B build/zcl-ledger-debug \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang \
  -DBLUE_ARM_GCC=/absolute/path/arm-none-eabi-gcc
cmake --build build/zcl-ledger-debug
ctest --test-dir build/zcl-ledger-debug --output-on-failure
```

`blue_sapling_out_open` uses the same ChaCha20 and Poly1305 arithmetic as
Z23's C23 host implementation, specialized to Sapling's 64-byte outgoing
plaintext, empty associated data, and zero nonce. It authenticates the
80-byte ciphertext before releasing plaintext. The function uses no heap,
thread, logging service, or external crypto library.

## Observation

On 2026-09-28T00:10:04-04:00 (2026-09-28T04:10:04Z), an AMD Ryzen 7 PRO
8840U host ran Clang 22.1.6 Debug with AddressSanitizer and
UndefinedBehaviorSanitizer. The committed consensus-accepted Sapling wire
passed six-pass replay, output selection, OCK derivation, and outgoing AEAD
authentication. The 64-byte `pk_d || esk` result matched Z23's separate
host ChaCha20-Poly1305 implementation byte for byte in the same CTest.
The host test links that core implementation directly as an independent
oracle; its support stubs affect only logging, time, fault injection, and
memory erasure. Altering an outgoing
ciphertext byte, its authentication tag, or the OCK failed and cleared the
plaintext result. GCC 16.1.1 Release passed the focused host test as well.
The complete Clang Debug Blue suite passed 53/53 CTests, and the local
`lint-fast` run passed all 33 gates.

ARM GNU Toolchain 16.2.0 compiled the decryptor as ISO C23 for Cortex-M0
with `-O2 -Wall -Wextra -Werror -pedantic`. Its isolated object occupied
2,276 bytes of `.text`, with no `.data` or `.bss`; its largest reported
static frame was 240 bytes. QEMU 11.0.1 executed the same public fixture
on Cortex-M0 and Cortex-M3. The outgoing-decrypt case's measured stack
peak was 736 and 632 bytes respectively, inside the 1536-byte test
ceiling. The full Cortex-M0 signing fixture's peak remained 1512 bytes.

## Limit

Only the outgoing ciphertext is decrypted. The device has not checked the
ephemeral public key, decrypted the note ciphertext, verified its
commitment, or displayed recipient, amount, and memo facts. This source is
not linked into the installed Blue app and has no approval or signing
route. QEMU stack measurements do not include BOLOS or SDK frames.
