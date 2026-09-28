<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Ledger Blue output touch replay

Local time: 2026-09-27T11:10:00-04:00

UTC: 2026-09-27T15:10:00Z

The SDK-shim test displays a complete transparent output page using the
wallet's C23 screen code. It checks the derived account label, formatted
value, and all three address lines. One CONTINUE tap acknowledges the output
and advances to the waiting page. A second tap through the old output
element leaves the review active, the acknowledgement count unchanged, and
the waiting page visible. After EXIT, another delayed tap leaves the app
closed. The existing code aborted review on a repeated CONTINUE tap.
The shim does not establish whether a queued touch from one output could be
delivered after the next output becomes visible. That event ordering needs
physical observation before any signer can rely on output acknowledgement.

The focused `blue-wallet-device-ui` test passed 1/1 under Clang 22.1.6
Debug with AddressSanitizer and UndefinedBehaviorSanitizer, and 1/1 under
GCC 16.1.1 Release. These host results do not prove physical touch event
ordering or BOLOS framebuffer behavior. The revision remains uninstalled.

Wallet 0.2.18 built against two independently patched Blue SDK trees at
revision `3c710b4c62ad847599a2deb0932a50dd1ae4bdff` with identical
34,048-byte `.text` SHA-256
`a2b78a3add93ca47177e2307c5c2ef50348240c2a8aff5d0bac37686425afd7a`.
The ARM GCC 16.2.0 size report is `.bss` 5,472 bytes and `.data` zero;
the stack gate measured a largest named C path of 752 bytes against a
2,048-byte stack reserve plus 512-byte margin. BOLOS firmware frames and
physical touchscreen behavior are not measured.
