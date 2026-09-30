<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Blue final signing page shows own-address amount

Local time: 2026-09-28T04:01:20-04:00

UTC: 2026-09-28T08:01:20Z

ZCL Wallet 0.3.11 shows the device-classified amount sent to its own fixed
P2PKH addresses on the final SIGN ZCL page. The page also shows the amount
sent to other addresses, the calculated fee, input path, branch ID, expiry
height, lock time, and CHAIN UNCHECKED warning. Immediately before displaying
the page, the device formats all three amounts from its completed review
state. The previous final page omitted the own-address amount after showing it
on the preceding totals page.

The C23 host SDK shell used distinct synthetic values of 1.50000000 ZCL to
others, 0.50000000 ZCL to the device's own addresses, and 1.00000000 ZCL fee.
It checked all three strings on the final page and pinned the 320 × 480 RGB
pixel SHA-256 to
`dbc06f359a629596132c8fb0fe9fff6c3b7f6781c8b6cff056b77a236a148621`.
The rendered page was inspected at its native resolution; the amount labels,
technical facts, warning, and NO SIGN / SIGN ZCL controls are legible without
overlap. The targeted device UI case passed with Clang 22.1.6 Release and
sanitized Debug on an AMD Ryzen 7 PRO 8840U. The complexity gate passed at
its unchanged cap of 15. The payment screen test also rendered the maximum
21,000,000 ZCL amount with the 22-pixel simulator font inside the 280-pixel
label width.

The reviewed Blue SDK built the image and passed its static stack gate. Intel
HEX SHA-256 was
`aec2fda55947224f8c1c31c0bb2fb1881e9e4f7011df6a52cadedc5af5e7f1f3`;
extracted `.text` SHA-256 was
`9ecd72a5227b9189e93dc1fb87a50bf406db0e30fa4d4b87c21d502e8a815727`.
The image contained 43,272 text bytes, zero initialized-data bytes, and
5,120 BSS bytes. The largest checked payment-upload stack path was 1,048
bytes plus a 512-byte margin; BOLOS frames are outside that calculation.

These simulator results do not prove physical touchscreen behavior. Version
0.3.11 remains uninstalled on a Ledger Blue.

A second source checkout at signed commit `08d69e1a2` built against a
separately patched copy of Blue SDK revision
`3c710b4c62ad847599a2deb0932a50dd1ae4bdff`. Its Intel HEX and
extracted `.text` SHA-256 values matched those above exactly. The SDK patch
SHA-256 was
`4919fd81ba7a3a80880065aaf7898c2edd603b2586f6086a88570c73996019f6`.
Both builds used one host and compiler toolchain. Independent-machine
reproducibility remains unverified. The full serial Blue CTest suite passed
53 of 53 cases in Release and 53 of 53 in sanitized Debug. Debug used
`ASAN_OPTIONS=detect_leaks=0` because LeakSanitizer cannot start under this
runner's ptrace environment; AddressSanitizer and UndefinedBehaviorSanitizer
remained active. The final source passed all 33 local `lint-fast` gates, the
core seal for 554 files and 80 sections, Markdown links for 494 documents
and 935 local targets, and the inline-path gate for 494 documents with 12
existing baseline entries and zero new failures.
