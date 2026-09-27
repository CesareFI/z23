<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Ledger Blue fixed-path input hash binding

Date: 2026-09-27.

## Question

Can the read-only Wallet refuse a previous P2PKH output that does not match
either public-key hash derived on the Blue, without accepting a host ownership
label or retaining more device state?

## Result

Wallet 0.2.8 passes the two derived HASH160 values into its APDU handler for
each command. INS `28` first verifies the complete previous transaction and
selected outpoint, then compares the selected P2PKH script's 20-byte hash
with both device-derived values. A mismatch or missing hash set returns
`6A80`, clears the review state, and returns no fee or digest. The hashes
remain outside the review state, so an abort does not erase the device's
derived public identity. The handler has no key or signing command.

The host fixture accepted a previous output with an external hash and one
with an internal hash. It rejected a third-party hash and a missing hash set.
Clang 22.1.6 Debug with AddressSanitizer and UndefinedBehaviorSanitizer and
GCC 16.1.1 Release each passed 26/26 local host tests. The repository
cyclomatic complexity report completed; the modified production paths
remain within the project cap of 15.

Two independent patched Ledger Blue SDK trees at
`3c710b4c62ad847599a2deb0932a50dd1ae4bdff` produced identical ARM
`.text` SHA-256
`022fb71c28a1720aaf7e19e8cc8824d9f30cd0a2dc364768109277596950f95d`
with Clang 22.1.6 and ARM GCC 16.2.0. The linked image has 30,720 bytes
of `.text`, 5,204 bytes of `.bss`, zero `.data`, and 940 bytes of app SRAM
after `.bss`. The largest named C stack path is 744 bytes against the
2,048-byte stack reservation and 512-byte margin. Measurements ran on an
AMD Ryzen 7 PRO 8840U with Radeon 780M Graphics.

## Limit

The hash match proves only that the supplied previous output script names
one of two fixed Blue-derived public keys. The Blue still does not prove the
output is in the accepted chain, unspent, mature, or controlled by the current
wallet policy. No signing command, physical Blue check of 0.2.8, or payment
approval occurred. The host must treat the fee and ZIP-243 digests as
read-only facts about the supplied bytes.
