<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Ledger Blue fixed internal address review

Date: 2026-09-27.

Wallet 0.2.7 derives the compressed public keys for
`m/44'/147'/0'/0/0` and `m/44'/147'/0'/1/0` after PIN validation. It retains
the first key for the receive command and the corresponding two public
HASH160 values for output classification. Only an exact P2PKH match to the
second device-derived value receives the `OWN INTERNAL 1/0` label. The
output page keeps the full 35-character address and shows the relation in
the Blue SDK's 22-pixel font. P2SH remains unlabeled as owned even if its
20-byte script hash equals either public-key hash.

The host simulator rendered a 320×480 dark-mode internal-address output page
to `zcl-blue-wallet-internal-output.png`; the file SHA-256 is
`37abc2209691faf3329dbeb14c64288c45babaf9d41b8a84f985d92b388db280`.
The visible label, amount, address, and buttons fit without overlap.
Clang 22.1.6 Debug with AddressSanitizer and UndefinedBehaviorSanitizer and
GCC 16.1.1 Release each passed 26/26 local host tests. The account classifier
tests cover external, internal, other P2PKH, P2SH, and missing-key states.
The cyclomatic complexity gate passed at cap 15.

The image built with Clang 22.1.6 and ARM GCC 16.2.0 from two separately
patched Ledger Blue SDK trees at
`3c710b4c62ad847599a2deb0932a50dd1ae4bdff`. Their ARM `.text` bytes
matched at SHA-256
`1eb30e77666749dedfe371c9cc1756d8658ef36b161bfae135ab7e2638130d2b`.
The linked image has 30,720 bytes of `.text`, 5,204 bytes of `.bss`, zero
`.data`, and 940 bytes of app SRAM after `.bss`. The named maximum C stack
path is 680 bytes against the 2,048-byte stack reservation and 512-byte
margin. No Blue installation or runtime derivation was performed.

An exact internal-address match does not establish that the transaction's
inputs belong to the same account, so the page does not call the output
change. Account selection, wider address indexes, final approval, and
payment signing remain unavailable.
