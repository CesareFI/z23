<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Ledger Blue completed-review touch handling

Local time: 2026-09-27T10:05:33-04:00

UTC: 2026-09-27T14:05:33Z

## Result

Wallet 0.2.17 ignores a second DONE tap and delayed BACK or TOTALS events
after read-only review completion. The SDK-shim touchscreen test invoked the
old button callbacks after the completion page appeared. The page remained
visible, no extra display was scheduled, and the signing approval flag stayed
clear. The fee, totals, and completion pixel hashes remained unchanged.
The focused UI test passed 1/1 with Clang 22.1.6 Debug/ASan/UBSan and GCC
16.1.1 Release on an AMD Ryzen 7 PRO 8840U.

The Ledger Blue SDK revision
`3c710b4c62ad847599a2deb0932a50dd1ae4bdff` with the exact reviewed C23
patch passed the device Makefile's source, `.data`, SRAM, and stack gates.
Two separate patched SDK copies and source worktrees built with Clang 22.1.6
and ARM GCC 16.2.0. Both produced 34,048
bytes of `.text`, 5,472 bytes of `.bss`, zero `.data`, and `.text` SHA-256
`386c39a9861431501e57c22fbb312a087f7623362a60800b6b602e229ff288bd`.
The largest named C stack path remained 752 bytes against a 2,048-byte
reservation and a separate 512-byte margin. BOLOS firmware frames are not
included in that measurement.

At 2026-09-27T10:08:17-04:00 (2026-09-27T14:08:17Z), both complete
Blue/ZCL host suites passed 30/30 tests with Clang 22.1.6 Debug/ASan/UBSan
and GCC 16.1.1 Release. The repository cyclomatic-complexity ratchet passed
60,787 functions at cap 15.

## Limit

The SDK-shim test models callbacks and pixels; it does not reproduce Blue
firmware touch delivery or USB timing. Wallet 0.2.17 is uninstalled and
read-only. No transaction signing or Sapling spend is enabled by this change.

At 2026-09-27T10:25:05-04:00 (2026-09-27T14:25:05Z), the SDK-shim UI test
also exercised cancellation from the waiting-for-chunks page, the
checking-inputs page, and the totals page. Each EXIT cleared the review and
approval state. Totals cancellation follows BACK then EXIT because its two
large touchscreen controls are BACK and DONE. The focused UI test passed
1/1 with Clang 22.1.6 Debug/ASan/UBSan and GCC 16.1.1 Release.
