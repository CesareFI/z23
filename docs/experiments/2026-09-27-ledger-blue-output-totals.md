<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Ledger Blue device-derived output totals

Date: 2026-09-27.

## Question

Can the Blue show how much of a reviewed transparent transaction pays its
two fixed addresses, how much pays other addresses, and the fee, without
accepting an ownership or change label from Z23?

## Result

Wallet 0.2.11 adds to an owned-output total only when a physical CONTINUE
acknowledges a parsed P2PKH output whose 20-byte hash matches one of the
Blue-derived public-key hashes. The total is provisional until three-pass
replay verifies the entire transaction. The verified public-output total
must cover it. After every input is bound, the TOTALS page displays that
owned total, the difference sent to other addresses, and the device-derived
fee. P2SH is counted with other addresses even when its script hash happens
to match a public-key hash. BACK returns to the input-path fee page; EXIT
ends review. Neither page approves or signs a payment.

Host tests covered one owned P2PKH output, a P2SH output with the same
20-byte hash that stayed outside the owned total, external and internal
input paths, rejected ownership hashes, and a missing hash set on CONTINUE.
Clang 22.1.6 Debug with AddressSanitizer and UndefinedBehaviorSanitizer and
GCC 16.1.1 Release each passed 26/26 local tests. The complexity ratchet
passed at cap 15. The expanded C23 stack gate includes post-reply amount
formatting and the TOTALS and BACK touch paths.

The C23 preview produced a 320×480 dark-mode totals screen at
`/tmp/zcl-blue-totals.png`, SHA-256
`1d09285f757b5a550d5278ab35860f56c631e6f07702faeef56c05239591653a`.
Visual inspection found the three values, chain warning, BACK, and EXIT
legible and nonoverlapping. Regenerate the preview with:

```sh
build/zcl-ledger/test-blue-payment-screen /tmp/zcl-output.png /tmp/zcl-p2sh.png /tmp/zcl-internal.png /tmp/zcl-fee.png /tmp/zcl-blue-totals.png
```

Two separately patched Ledger Blue SDK trees at
`3c710b4c62ad847599a2deb0932a50dd1ae4bdff` produced identical ARM
`.text` SHA-256
`eca26a7cb7465cbbe65164a0b37bcd7cc43a8d882db59654620e186e9b27606d`
with Clang 22.1.6 and ARM GCC 16.2.0. The image has 32,512 bytes of
`.text`, 5,296 bytes of `.bss`, zero `.data`, and 848 bytes of app SRAM
after `.bss`. The largest named C stack path is 752 bytes against the
2,048-byte reservation and 512-byte margin. Measurements ran on an AMD
Ryzen 7 PRO 8840U with Radeon 780M Graphics.

## Limit

The preview uses the Blue SDK font bitmap and matching coordinates; it does
not prove physical touch behavior or display timing. The fixed public-key
hashes do not establish chain inclusion, unspent status, maturity, the active
branch, or wider account ownership. The label describes those two fixed
addresses and does not assert that an output is change. Wallet 0.2.11 has no
signing command and has not been installed on the Blue.
