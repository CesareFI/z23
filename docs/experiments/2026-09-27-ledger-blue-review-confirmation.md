<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Ledger Blue read-only review confirmation

Local time: 2026-09-27T05:26:22-04:00

UTC: 2026-09-27T09:26:22Z

## Question

Can a physical review confirmation bind ordered ZIP-243 digests to all
verified transparent inputs without growing the Blue's limited SRAM or
allowing a USB command to confirm on the user's behalf?

## Result

Wallet 0.2.13 reuses each input's 36-byte outpoint slot after its previous
transaction has been checked. The slot then holds the device-derived 32-byte
ZIP-243 digest and a one-byte external or internal derivation path. The
original sequence is cleared. No digest can be consumed until the full
transaction, every output, every previous output, and the fee are verified
and a final touchscreen callback confirms the totals page. It also rechecks
the fee equation and every input's path code. The latch returns
one digest at a time in input order, clears each consumed slot, and refuses
duplicate, out-of-order, or second approval attempts. USB commands cannot
call this callback or consume a stored digest. INS `28` still returns each
computed digest before confirmation for host comparison.

The actual device screen source shows BACK and CONFIRM on the totals page;
CONFIRM shows REVIEW CONFIRMED and NO SIGNING. The C23 host UI harness follows
this touch flow and rejects an attempt to confirm before opening totals.
The fee, totals, and confirmed screenshots were inspected at 320×480:

| Page | SHA-256 of PNG |
| --- | --- |
| Fee | `2609788f7ff98111bd8aaef24d482e30a62774a3dad84f52b7540e2b4f5137cd` |
| Totals | `ba2a66c7e0d9309305cba9a0306e5de4e9c82a30441f86281a0f94358867e6d3` |
| Confirmed | `c10414e339543e9f31db6217937a10af656d8e35d799be5ace705b819d41e049` |

Regenerate screenshots with:

```sh
cmake -S apps/zcl-ledger -B build/zcl-ledger -DCMAKE_BUILD_TYPE=Debug
cmake --build build/zcl-ledger --target test-blue-wallet-device-ui
build/zcl-ledger/test-blue-wallet-device-ui \
  /tmp/zcl-fee.png /tmp/zcl-totals.png /tmp/zcl-confirmed.png
```

Clang 22.1.6 Debug with AddressSanitizer and UndefinedBehaviorSanitizer and
GCC 16.1.1 Release each passed 27/27 local tests. Two independently patched
Ledger Blue SDK trees at revision `3c710b4c62ad847599a2deb0932a50dd1ae4bdff`
produced identical ARM `.text` SHA-256
`079b0606e90c3bf55f9c28d8faa3546b7e866fda7bb61801865762a0f5ec8de0`.
The image has 33,536 bytes of `.text`, 5,296 bytes of `.bss`, zero `.data`,
and 848 bytes of app SRAM after `.bss`. The largest named C stack path is
752 bytes against the 2,048-byte reserve and 512-byte margin; the new
confirmation touch path measures 184 bytes. CPU: AMD Ryzen 7 PRO 8840U with
Radeon 780M Graphics. Test date: 2026-09-27.

## Limit

No APDU exposes a signature or reads the confirmation latch, and the app still cannot
sign a transaction. The host UI harness does not emulate BOLOS touch timing
or USB. Wallet 0.2.13 remains uninstalled; the physical Blue must pass
receive, review, EXIT, interruption, and confirmation tests before signing
is enabled. This confirmation is a read-only UX rehearsal, not authorization
to spend funds.
