# Ledger Blue final signing facts

Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.

## User-visible result

The final transparent signing screen displays the branch ID selected by the
device replay, plus the parsed transaction expiry height and lock time. These
values appear below the input path and above the signing choice. The screen
retains the chain verification warning because the device does not verify the
host's active chain tip.

The screen aborts the payment if the review is unverified, the fee is not
ready, replay has not completed all four passes, a previous transaction is
still active, or a field cannot fit its display buffer.

## Reproducible check

From a Clang 22 Debug build configured with `BLUE_ARM_GCC` for the local ARM
toolchain:

```sh
cmake --build /tmp/z23-blue-ui-next-debug --target test-blue-wallet-device-ui -j2
ctest --test-dir /tmp/z23-blue-ui-next-debug -R '^blue-wallet-device-ui$' --output-on-failure
```

On 2026-09-28, the test passed. The simulator's final signing screen RGB
SHA-256 was
`acdd06c721b37ff75bf379e9429cb82138bc55b86a87917e28dc464832a2db6d`.
The rendered 320 × 480 image was inspected for text overlap and button
visibility. The test also checks the full `uint32_t` formatting range and
that an incomplete replay ends the review without signing approval.

The app also compiled and linked with Clang 22.1.6 against the reviewed Blue
SDK revision and C23 patch. Its image gate passed with zero initialized data,
5,120 bytes of BSS including the reserved stack, and a 1,056-byte maximum
statically calculated payment-upload call path plus the required 512-byte
margin. This image has not been installed on the physical Blue.

Two local checkouts with the same Blue app source and reviewed SDK patch
produced byte-identical Intel HEX files. The HEX SHA-256 is
`4a30467521c05d7534d64484e9e41017c6a6c4653c2418a6bbd5735cd33234d7`;
the extracted loadable `.text` SHA-256 is
`3796f8ebafe429809b5c4c999c60437eee75a8744e9a5f4ab1ca4ec0cf2e6636`.
The surrounding Z23 commits differ in unrelated files, so this comparison
checks the app build across worktrees but is not a complete clean-room
reproduction of the repository.

The SDK compile caught an unbraced `UX_DISPLAY` call that the prior host stub
accepted because it modeled the SDK's multiple-statement macro as one
statement. The call now has braces, and the stub preserves that macro shape.
The same inspection found an unbraced timeout callback in the Blue event
handler. It now changes to the receive screen only when the payment approval
actually expires. The device UI and startup host tests passed after these
changes, followed by the SDK image and stack gates.
