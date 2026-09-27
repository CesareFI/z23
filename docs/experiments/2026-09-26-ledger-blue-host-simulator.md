<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Ledger Blue Review host simulation and stack gate

Recorded: 2026-09-26T20:49:50-04:00 / 2026-09-27T00:49:50+00:00.

## Objective

Exercise the Review app's command-to-screen transitions before another
device installation. Reject ARM images whose measured call paths exceed
the SDK stack reserve with 512 bytes of remaining space.

## Method and observations

The Blue build and host simulator compile the same C23
`blue_review_app.c` controller. The simulator sends identity, begin,
two transaction chunks, ZIP-243 digest, and summary commands for the
published 245-byte transparent fixture. It advances through the summary,
both public output pages, and the summary again. It checks clear and
pending-review behavior. The printed pages show 0.49999755 ZCL total,
0.40000000 and 0.09999755 ZCL outputs, and two independently derived
mainnet addresses.

Clang 22.1.6 Debug with AddressSanitizer and UndefinedBehaviorSanitizer:
13/13 CTest cases passed. GCC 16.1.1 Release: 13/13 passed. The Blue ARM
build used Clang 22.1.6 and `arm-none-eabi-gcc` 16.2.0 with the patched
Ledger `blue-r21.1` SDK. Its `.text` was 29,696 bytes, `.data` zero,
and `.bss` 6,024 bytes. The extracted `.text` SHA-256 was
`199d31d71684eaa0ae7185228d42aad591f9ac869b3aa5198ff3642c650a90ed`.
The stack gate measured a 768-byte APDU path and 936-byte screen path
against a 2,048-byte SDK reserve and a 512-byte margin. It rejected the
same object stack measurements against the base 1,024-byte SDK reserve.

## Limits and next experiment

The previous Review 0.4.0 lockup happened after an otherwise successful
host test. The new simulator does not execute BOLOS, USB interrupts,
touchscreen rendering, or EXIT. The measured stack paths omit unknown SDK
frames and interrupt nesting. The observed lockup cause remains unknown;
stack pressure is a hypothesis. Review 0.4.1 was not installed or added
to the installer's allowlist. A compatible Ledger Blue firmware emulator
or equivalent BOLOS-level test is needed before another device trial.
