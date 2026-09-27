<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Ledger Blue readable review candidate

Recorded: 2026-09-26T23:18:14-04:00 / 2026-09-27T03:18:14+00:00.

## Objective

Provide larger transaction text and a dark display option on the Blue while
preserving the read-only Review app's APDU behavior, explicit EXIT control,
and a bounded memory footprint. Inspect every screen offline before any
physical install.

## Result

Review 0.4.3 adds LARGER TEXT, STANDARD TEXT, DARK, and LIGHT controls.
The 22-pixel BAGL alphabet displays one review detail at a time. A shared
C23 formatter wraps complete words and inserts a line break into an
unspaced value only when necessary. The formatter is compiled into both
the ARM app and the PNG renderer. The device candidate and host controller
share the detail and palette state. NEXT DETAIL cycles through the initial
connection instructions, then through every reviewed transaction detail
and public output page. The default six-line layout remains available.

The published ZIP-243 vector 3 generated 29 large-text dark PNGs. Visual
inspection of the warning page found `SHIELDED HIDDEN; NO` and `SIGNING`
on separate complete lines, with visible text-size, palette, navigation,
and EXIT controls. A 31-character `W` detail exercised the widest glyphs
without truncation in the PNG renderer. The CLI regression checks all PNG
signatures and 320 × 480 dimensions. Host tests check detail navigation
before and after review, both palette transitions, and word wrapping.

Clang 22.1.6 Debug with AddressSanitizer and UndefinedBehaviorSanitizer
passed 14/14 local CTest cases; GCC 16.1.1 Release passed 14/14. The host
CPU was an AMD Ryzen 7 PRO 8840U. The ARM C23 build produced `.text` 32,256
bytes, `.data` 0 bytes, and `.bss` 6,000 bytes including its 2,048-byte
stack reserve. The `.text` SHA-256 is
`182f749bbe1166ff9bef10a0faf4661411d4ae851b5afeb218b2e7f60e5d6f37`.
The linker has 144 bytes of app SRAM headroom. The stack gate measured
768 bytes for the APDU path and 920 bytes for the screen path, plus its
512-byte margin. The repository complexity gate passed at cap 15.

The APDU transaction limit is 2,304 bytes, reduced by 128 bytes from 0.4.2
to fund the accessible screen's scratch element and wrapped text. This is
a read-only prototype limit; a final signer needs bounded streaming for
larger transactions.

## Limits and next experiment

The renderer uses Ledger's open SDK font data and layout but approximates
rounded pixels and the color palette. The dedicated Blue's BOLOS 2.1.1
font bytes and touch handling have not been independently captured. Review
0.4.3 was not pinned, installed, opened, or contacted over USB. No physical
screen, tap, palette, EXIT, or timing claim follows from these offline
checks. The prior 0.4.0 image froze during a physical review, so the next
device experiment must start with install and EXIT on the dedicated test
Blue, then test each control and synthetic transaction page while watching
for USB responsiveness. A failure blocks promotion and requires a device
restart before further commands.

This app still has no signing command or keys. It cannot validate Sapling
recipients or amounts, transparent input ownership or fee, multisig policy,
ZSLP lineage, or ZCL consensus. The host and device work needed for those
capabilities is tracked in the [wallet roadmap](../../apps/zcl-ledger/ROADMAP.md).
