<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Blue payment abort erasure

Local time: 2026-09-28T03:49:35-04:00

UTC: 2026-09-28T07:49:35Z

ZCL Wallet 0.3.10 uses volatile byte stores when it aborts a payment review,
consumes a per-input ZIP-243 digest, or clears the device hash workspaces and
payment text. A failed output-page CONTINUE tap now aborts the entire session
and displays REVIEW ENDED. Earlier code reset the payment parser on that touch
failure but left the screen controller's visible state and hash workspaces.

The host SDK shell fills the hash contexts and payment text with nonzero bytes,
aborts, and checks each byte is zero. It also makes a displayed output touch
fail after account readiness is removed, then checks that no approval or
payment remains visible. The focused payment review, signing, and device UI
tests passed in Clang 22.1.6 Release and Debug on an AMD Ryzen 7 PRO 8840U.
Debug used AddressSanitizer and UndefinedBehaviorSanitizer with
`ASAN_OPTIONS=detect_leaks=0` because LeakSanitizer cannot start under this
runner's ptrace environment. The final Blue CTest suite passed 53 of 53 cases
in Release and 53 of 53 in sanitized Debug. The cyclomatic complexity gate
passed at its unchanged cap of 15.

The reviewed Blue SDK build passed its image and static stack gates. The Intel
HEX SHA-256 was
`68ea953554fd6542320de86ba76317b690b6724e1b86bba5c03222e27dcfbc0b`;
the extracted `.text` SHA-256 was
`7b71eccfeb9fd88334fb95d4d3f78a23eb7f90451724ec46fd272a040d4c806b`.
The linked image contained 43,016 text bytes, zero initialized-data bytes,
and 5,120 BSS bytes. The largest statically checked payment-upload stack path
was 1,048 bytes plus a 512-byte margin; BOLOS frames are outside that
calculation. ARM disassembly of `blue_payment_apdu_abort` contains a loop of
`strb` instructions covering the 1,264-byte state, followed by the failed
flag store. `blue_payment_apdu_take_digest` contains explicit byte stores over
the 36-byte consumed record.

The host checks and disassembly establish erasure in this compiled image.
They do not establish erasure after abrupt power loss or through firmware
frames. The image has not run on a physical Blue.

A second source checkout at signed commit `e46a1b054` built against a
separately patched copy of Blue SDK revision
`3c710b4c62ad847599a2deb0932a50dd1ae4bdff`. Its Intel HEX and
extracted `.text` SHA-256 values matched the first build exactly. The SDK
patch SHA-256 was
`4919fd81ba7a3a80880065aaf7898c2edd603b2586f6086a88570c73996019f6`.
Both builds used one host and compiler toolchain; independent-machine
reproducibility remains unverified. The final source passed all 33 local
`lint-fast` gates, the core seal for 554 files and 80 sections, Markdown links
for 493 documents and 935 local targets, and the inline-path gate for 493
documents with 12 existing baseline entries and zero new failures.
