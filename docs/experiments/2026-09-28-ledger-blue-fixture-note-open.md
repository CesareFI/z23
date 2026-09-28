<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Sapling output fixture agreement and note opening

## Intention

Check the full public test-fixture path from the consensus-accepted Sapling
transaction bytes through selected-output capture, outgoing recovery,
Jubjub key agreement, note-key derivation, and authenticated note opening.
Do not route a device secret through the unvalidated scalar multiplier.

## Reproduction

```sh
cmake -S apps/zcl-ledger -B build/zcl-ledger-debug \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang
cmake --build build/zcl-ledger-debug
ctest --test-dir build/zcl-ledger-debug --output-on-failure
```

`test-blue-sapling-ock` reads the committed 1,425-byte deterministic simnet
fixture. Its six-pass replay matches Z23's recorded ZIP-243 digest and captures
the sole output. The fixture OVK authenticates and opens its 80-byte outgoing
ciphertext to a public test `pk_d || esk`. The Blue C23 decoder validates
`pk_d`; the Blue arithmetic multiplies it by cofactor eight, then by the
fixture's `esk`. Z23's separate Jubjub implementation computes the same
32-byte agreement. The Blue KDF derives the note key from the agreement and
captured `epk`; Blue AEAD and Z23's separate ChaCha20-Poly1305 implementation
open the same 580-byte ciphertext. The recovered note has type `0x01`,
diversifier `9cf4941906e9f1951a9199`, and little-endian value `99990000`
zatoshis. Changing `epk` changes the key and the AEAD refuses the note while
clearing its plaintext buffer.

## Observation

On 2026-09-28T01:01:34-04:00 (2026-09-28T05:01:34Z), an AMD Ryzen 7 PRO
8840U running Clang 22.1.6 Debug with AddressSanitizer and
UndefinedBehaviorSanitizer passed the focused test. GCC 16.1.1 Release passed
the same focused test. The complete Clang Blue suite passed 53/53 CTests,
including both Cortex-M0 and M3 QEMU images. The staged cyclomatic-complexity
gate scanned 63,129 functions at cap 15 and passed.
The repository's `make lint-fast` passed all 33 gates.

## Limit

This is a public, deterministic simnet fixture in a host test. The Blue app
does not yet derive agreement from a device secret, independently validate
the diversifier, derive and compare `epk`, recompute the note commitment, or
authorize shielded signing. The scalar multiplier has no target timing
validation for secret scalars. The decrypted amount is not a device-verified
payment fact.
