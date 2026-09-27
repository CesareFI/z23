<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Linked Blue Wallet read-only review candidate

Local time: 2026-09-27T01:45:39-04:00

UTC: 2026-09-27T05:45:39Z

## Question

Can the Wallet retain its fixed-path transparent receive function while
linking an output-by-output, read-only ZCL transaction review within the
Blue's app SRAM and stack limits, using reproducible C23 build inputs?

## Method and evidence

Wallet 0.2.0 links the bounded unsigned v4 parser, three-pass ZIP-243 replay,
payment APDU state machine, output formatter, and BAGL touch screens. USB
commands can begin, feed, advance passes, finish, cancel, and query a review.
The sole acknowledgement call is in the touchscreen CONTINUE callback. Each
output screen shows its exact amount, all 35 mainnet address characters,
position, type, DRAFT; NO SIGNING, CONTINUE, and EXIT. A nonpayment APDU
aborts an active review before the receive command runs. The host accepts
both the pinned v8 receive-only and new v9 receive/review app identities.

The build used Clang 22.1.6 and ARM GCC 16.2.0 on AMD Ryzen 7 PRO 8840U.
The SDK base revision is `3c710b4c62ad847599a2deb0932a50dd1ae4bdff`.
The repository SDK patch has SHA-256
`58661b642ec4151713eae673e8359b0f6d44fbad2981298290aea7ea1d6e7e23`.
That patch sets the linker stack reserve to 2,048 bytes and contains the
reviewed C23 and USB transport adaptations. A fresh SDK worktree at the
base revision accepted the patch and produced the same SDK diff hash as the
original patched checkout. Clean builds against both SDK directories
produced identical 24,576-byte `.text` images with SHA-256
`d69f5f07cf2280aeb253f9ff91d494f78bd8b635d0f45d4c778b1d5137251bdb`.
The installer accepts that hash and rejects a one-byte changed image before
opening USB. No install occurred during this experiment.

The linked ELF has zero `.data` and 4,236 bytes of `.bss`. The SDK app SRAM
region is 6,144 bytes; `.bss` includes the 2,048-byte linker stack reserve.
The image leaves 1,908 bytes after `.bss`, above its 512-byte headroom guard.
The named C stack-path gate sums 608 bytes for output upload, 648 for screen
formatting, 632 for replay finish, 472 for pass advance, and 80 for touch,
all within the 2,048-byte reserve with a 512-byte margin. These path sums
exclude BOLOS firmware frames. Clang Debug address/undefined-behavior and
GCC Release host suites each passed 22/22 tests. The payment test includes
10,000 deterministic malformed/interleaved APDUs and a simulated USB abort.
The cyclomatic gate passed at cap 15. The host 320 × 480 dark preview was
visually checked with the linked app's dark colors and CONTINUE/EXIT layout.

## Limits

This is an uninstalled candidate. Host APDU tests and SDK linking do not
prove BOLOS USB timing, touchscreen callbacks, screen pixels, EXIT behavior,
or recovery after interrupted USB on the physical Blue. The local Blue
Speculos checkout supports SDK 1.5 and blue-2.2.5, not this app's Blue 2.1.x
SDK; it cannot be used as exact firmware evidence. The device does not
authenticate a prevout's chain status, fee, change, account, or active
consensus branch. The final replay digest is discarded. No payment signing
or Sapling spend command exists. Physical testing must confirm this exact
image hash before any claim of device readiness.
