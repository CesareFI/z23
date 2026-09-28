<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Selected Sapling note ciphertext capture

## Intention

Supply the encrypted note bytes to the device-side verifier from the same
selected output and six-pass replay as its public fields and outgoing
ciphertext. Keep the capture provisional until every upload matches.

## Reproduction

```sh
cmake -S apps/zcl-ledger -B build/zcl-ledger-debug \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang
cmake --build build/zcl-ledger-debug
ctest --test-dir build/zcl-ledger-debug --output-on-failure
```

The selected output capture now holds the first 756 bytes of its 948-byte
wire record: `cv`, `cm`, `epk`, 580-byte note ciphertext, and 80-byte
outgoing ciphertext. It excludes the 192-byte proof. The streaming
observer copies only the requested output and only during the first pass;
the following five complete passes must match the first wire commitment.

## Observation

On 2026-09-28, the focused Clang 22.1.6 Debug sanitizer tests passed with
1- and 220-byte chunks, first and second output selection, a changed note
ciphertext byte in each later pass, and abort clearing. The committed
consensus-accepted Sapling fixture matched all captured ciphertext bytes
and its independently expected ZIP-243 digest. The full Blue suite passed
53/53 CTests. GCC 16.1.1 Release passed both focused tests, and the
33-gate `make lint-fast` run passed. The host CPU was an AMD Ryzen 7 PRO
8840U.

## Limit

These are encrypted public wire bytes. Replay identity alone does not
prove a recipient, value, memo, or note commitment. The enlarged capture
has not been linked into an installed Blue app or measured in its BOLOS
memory layout.
