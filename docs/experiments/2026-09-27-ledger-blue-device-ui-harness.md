<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Ledger Blue wallet device UI harness

Local time: 2026-09-27T05:16:41-04:00

UTC: 2026-09-27T09:16:41Z

## Question

Can the wallet's actual BAGL screen definitions and touch callbacks be
checked on the host before a physical Blue installation?

## Result

Version 0.2.12 adds a C23 test that compiles the device screen source with
small SDK shims. It checks element bounds and text fit for the fee, totals,
and ended pages. It follows TOTALS, BACK, and EXIT callbacks; confirms that
the displayed fee, other-output total, own-output total, and input path read
from the wallet's review state; and verifies that impossible output totals
end the review. The short warning labels and button text now use the Blue
SDK's 22-pixel Open Sans Light font. The fee and totals screenshots were
visually inspected at 320×480. They showed no overlapping or clipped text.

The C23 renderer produced:

| Page | SHA-256 of PNG |
| --- | --- |
| Fee | `2609788f7ff98111bd8aaef24d482e30a62774a3dad84f52b7540e2b4f5137cd` |
| Totals | `31ca25fc35f003cdaab93cbb7e0e8471dfafbba49dc943be2931bfd22d4de32d` |

At commit `5d87be535`, regenerate both images with:

```sh
cmake -S apps/zcl-ledger -B build/zcl-ledger -DCMAKE_BUILD_TYPE=Debug
cmake --build build/zcl-ledger --target test-blue-wallet-device-ui
build/zcl-ledger/test-blue-wallet-device-ui /tmp/zcl-fee.png /tmp/zcl-totals.png
```

Later versions add a third screenshot argument for the confirmation page;
their image hashes are recorded in the confirmation experiment.

Clang 22.1.6 Debug with AddressSanitizer and UndefinedBehaviorSanitizer and
GCC 16.1.1 Release each passed 27/27 local tests. Two independently patched
SDK trees at Ledger revision `3c710b4c62ad847599a2deb0932a50dd1ae4bdff`
produced identical ARM `.text` SHA-256
`8fc5957810c7d2a77f01e4deb7046206b061b8ffcb1dcdac96dbaad4deed0a64`.
The image has 32,512 bytes of `.text`, 5,296 bytes of `.bss`, zero `.data`,
and a largest named C stack path of 752 bytes against the 2,048-byte
reservation and 512-byte margin. CPU: AMD Ryzen 7 PRO 8840U with Radeon
780M Graphics. Test date: 2026-09-27.

## Limit

The host shims do not emulate BOLOS event timing, firmware drawing, or USB.
The renderer uses the SDK 22-pixel font bitmap but approximates the 14-pixel
font and button chrome. Its fit checks and screenshots do not prove physical
touch response. The crypto shims fail if called; cryptographic review remains
covered by separate APDU tests. Version 0.2.12 is read-only and uninstalled.
