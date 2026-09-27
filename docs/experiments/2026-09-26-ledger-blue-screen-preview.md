<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Ledger Blue C23 screen preview

Recorded: 2026-09-26T22:57:20-04:00 / 2026-09-27T02:57:20+00:00.

## Objective

Make Review app pages inspectable before asking the owner to install an ARM
image on the dedicated Ledger Blue. Reject clipping in the host preview and
keep physical device claims separate from simulated claims.

## Method and result

`zcl-blue-screen-sim` feeds a raw transaction through the C23 Review app
controller's identity, begin, chunks, and summary commands. It captures the
waiting screen and each page transition. The PNG renderer uses the same
320 × 480 layout constants as the ARM app. It decodes Ledger's Apache-2.0
Open Sans 11/14 BAGL 4-bit font table and rejects text wider than its label.
The published ZIP-243 transparent vector 3 produced five 320 × 480 PNGs:
waiting, summary, two public output pages, and summary wraparound. Visual
inspection of the summary PNG found all six labels and two buttons legible
without clipping. The CLI test checks the PNG signature and dimensions.
The accessibility preview rendered 29 PNGs from the same fixture with the
SDK's 22-pixel full-alphabet font and a dark palette. It placed each nonempty
detail on a separate screen, wrapped text without truncation, and retained
NEXT DETAIL and EXIT controls. Visual inspection found the long shielded
status split between complete words. These styles are host previews only.

Clang 22.1.6 Debug with AddressSanitizer and UndefinedBehaviorSanitizer
passed 14/14 CTest cases; GCC 16.1.1 Release passed 14/14. The host CPU
was an AMD Ryzen 7 PRO 8840U. The ARM app rebuilt with `.text` 29,696 bytes,
`.data` 0 bytes, and `.bss` 6,024 bytes. Its `.text` SHA-256 remained
`1df563d4ef89cedaa7be4513c8697e729b1c7a8cbca8fb3261cb5203a8f0a88e`;
moving layout constants into a shared header did not change the ARM image.
The 2,048-byte stack reserve passed the existing APDU 768-byte and screen
928-byte path check with its 512-byte margin. The repository cyclomatic
complexity gate passed at cap 15.

## Limits and next measurement

Ledger's published [Blue hardware](https://www.ledger.com/ledger-blue-an-enterprise-grade-security-device)
identifies the ST31G480 secure element; [ST's product data](https://www.st.com/en/secure-mcus/st31g480.html)
lists up to 28 MHz, 12 KiB user RAM, and 480 KiB secure user flash. Those
are chip maxima, not observed Blue app clock speed or free storage. The
patched Blue linker exposes 6 KiB app SRAM, leaving 120 bytes after this
build's `.bss`; code is 29,696 bytes in a 400 KiB link address range.
App transaction review is currently limited to 2,432 bytes. On-device
latency has not been measured.

The font table comes from Ledger's open Nano S SDK. The exact BOLOS 2.1.1
font bytes and Blue display pipeline have not been captured. Rounded button
edges and the antialias palette are approximated. A future safe comparison
should photograph or capture a known static screen from the Blue, compare
glyph positions and colors, and only then claim pixel agreement. This work
neither installs an app nor sends a USB command to the Blue. Review 0.4.2
remains an unpinned offline candidate after the Review 0.4.0 freeze.
