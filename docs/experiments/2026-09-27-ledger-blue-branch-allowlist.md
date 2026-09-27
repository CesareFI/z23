<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Ledger Blue mainnet branch allowlist

Date: 2026-09-27.

## Question

Can the Blue reject an arbitrary host-supplied ZIP-243 branch ID before
accepting a transaction review, while keeping its mainnet branch values in
the same C23 source used by the host?

## Result

Wallet 0.2.9 links the host's `blue_mainnet_branch.c` into the Blue app.
INS `20` requires its branch ID to equal one of the ZCL mainnet Sapling,
Bubbles, or Bubbly branch IDs from `core/params/src/upgrades.c`. An unknown
ID returns `6A80` and clears review state. This check does not select the
branch: the host still supplies one of the three known IDs.

Tests covered the three known IDs, zero, all-one bits, and a complete review
begin rejected with an unknown ID. Clang 22.1.6 Debug with AddressSanitizer
and UndefinedBehaviorSanitizer and GCC 16.1.1 Release each passed 26/26
local host tests. The cyclomatic complexity ratchet passed at cap 15.

Two separately patched Ledger Blue SDK trees at
`3c710b4c62ad847599a2deb0932a50dd1ae4bdff` produced matching ARM
`.text` SHA-256
`e03f2bb63517f550d6cff912d5dc851b1e1874c7bfd50cf7aba3eb91a504fa40`
with Clang 22.1.6 and ARM GCC 16.2.0. The image has 30,976 bytes of
`.text`, 5,204 bytes of `.bss`, zero `.data`, and 940 bytes of app SRAM
after `.bss`. The largest named C stack path is 744 bytes against the
2,048-byte reservation and 512-byte margin. Measurements ran on an AMD
Ryzen 7 PRO 8840U with Radeon 780M Graphics.

## Limit

The Blue cannot independently know the current mainnet height from an
uploaded transaction. This allowlist does not prove that the supplied known
branch is active, or that a previous output is included, unspent, or mature.
Wallet 0.2.9 has no signing command and has not been installed or checked on
the physical Blue.
