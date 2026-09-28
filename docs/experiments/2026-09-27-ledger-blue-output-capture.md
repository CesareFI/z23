<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Selected Sapling output capture

## Intention

Recover the public wire fields needed to open a selected Sapling outgoing
ciphertext without retaining a complete transaction on the Blue. Release
them only after the six-pass ZIP-243 replay validates identical uploads.

## Reproduction

```sh
cmake -S apps/zcl-ledger -B build/zcl-ledger-debug \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang
cmake --build build/zcl-ledger-debug
ctest --test-dir build/zcl-ledger-debug --output-on-failure
```

The selected output capture contains 32-byte `cv`, `cm`, and `epk` fields
plus 80 bytes of outgoing ciphertext. The streaming parser extracts those
bytes from the first upload. Five later uploads must match the first
transaction's full-wire SHA-256 commitment; any mismatch clears the
capture. The digest calculation remains the existing six-pass ZIP-243
implementation.

## Observation

On 2026-09-27T23:56:13-04:00 (2026-09-28T03:56:13Z), an AMD Ryzen 7 PRO
8840U host compiled the replay with ARM GNU Toolchain 16.2.0 for
Cortex-M0 at `-std=c23 -Os -Wall -Wextra -Werror -pedantic`. The isolated
object occupied 1,524 bytes of `.text` with no `.data` or `.bss`; its largest
reported static frame was 104 bytes. Host Clang measured 576 bytes for
the replay state, 664 bytes for the read-only APDU controller, and 176 bytes
for the caller-owned capture. The selected-output fixture checks first and
second output positions, 1- and 220-byte chunks, rejection of a missing
output index, full-wire substitution in each of passes two through six,
and erasure on abort. The committed consensus-accepted one-spend fixture
also matched the captured fields and ZIP-243 digest. The complete Clang
Debug sanitizer suite passed 53/53 CTests; the repository's
cyclomatic-complexity check passed at cap 15.
The existing read-only Shielded Review Blue image also built with the
patched Blue SDK, Clang 22.1.6, and ARM GCC 16.2.0. Its stack gate reported
824 bytes for the largest modeled finish path against a 1536-byte ceiling.
The new capture API is not called by that image.

## Limit

The captured data is public transaction wire, even after the replay
commitment succeeds. It does not prove a recipient, amount, memo, or note
commitment. The Blue app does not yet derive an outgoing cipher key, decrypt
the output, show those facts to the user, or approve shielded signing.
