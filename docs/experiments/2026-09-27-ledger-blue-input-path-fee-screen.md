<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Ledger Blue verified input paths on the fee screen

Date: 2026-09-27.

## Question

Can Wallet show the derivation paths of transparent inputs only after their
previous-output scripts match Blue-derived public-key hashes, while keeping
the final screen legible within the Blue's memory budget?

## Result

Wallet 0.2.10 records an external or internal path bit after each previous
transaction passes its outpoint, script-hash, amount, and ZIP-243 checks.
The bits are cleared on abort. A fee page appears only after all inputs are
bound; it shows `m/44'/147'/0'` and `INPUT EXT 0/0`, `INPUT INT 1/0`, or
`INPUT 0/0 + 1/0`, as appropriate. An invalid or absent path mask prevents
the fee page from appearing. This is a device-derived label, not host text.

Host tests covered external-only, internal-only, mixed, rejected foreign
inputs, missing hash sets, and invalid path labels. Clang 22.1.6 Debug with
AddressSanitizer and UndefinedBehaviorSanitizer and GCC 16.1.1 Release each
passed 26/26 local tests. The cyclomatic complexity ratchet passed at cap 15.
The C23 preview produced a 320×480 dark-mode mixed-path fee page at
`/tmp/zcl-blue-fee-mixed.png`, SHA-256
`34bf858cd34a176df4a23c75e066aee17b7b9a52d8ada58d0f611c1477e1d459`.
Visual inspection found the fee, two path lines, three warnings, and EXIT
button legible and nonoverlapping in that preview. Regenerate it with:

```sh
build/zcl-ledger/test-blue-payment-screen /tmp/zcl-output.png /tmp/zcl-p2sh.png /tmp/zcl-internal.png /tmp/zcl-blue-fee-mixed.png
```

Two separately patched Ledger Blue SDK trees at
`3c710b4c62ad847599a2deb0932a50dd1ae4bdff` produced identical ARM
`.text` SHA-256
`10a75e3f4edd39c0e60d0ae0956e6f6da1901c38af53f53d73cd3b83dfd9cc23`
with Clang 22.1.6 and ARM GCC 16.2.0. The image has 31,232 bytes of
`.text`, 5,224 bytes of `.bss`, zero `.data`, and 920 bytes of app SRAM
after `.bss`. The largest named C stack path is 744 bytes against the
2,048-byte reservation and 512-byte margin. Measurements ran on an AMD
Ryzen 7 PRO 8840U with Radeon 780M Graphics.

## Limit

The preview uses the Blue SDK font bitmap and matching coordinates; it does
not prove the physical Blue's touch response or display timing. The fixed
path label does not prove chain inclusion, unspent status, maturity, or the
active consensus branch. Wallet 0.2.10 has no signing command and has not
been installed on the Blue.
