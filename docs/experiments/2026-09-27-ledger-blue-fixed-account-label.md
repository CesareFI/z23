<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Fixed-account output label in the Blue review candidate

Date: 2026-09-27. Wallet 0.2.4 copies the 20-byte P2PKH hash derived from
its fixed BIP44 account only after the receive address has been formatted.
The read-only payment command refuses to start if that derivation did not
succeed. For a pending output, the shared C23 screen code compares all 20
hash bytes: an exact P2PKH match shows `THIS ACCOUNT`; a different P2PKH
hash shows `OTHER ADDRESS`; P2SH shows `P2SH ADDRESS` even if its hash bytes
match. The latter is a script hash, not evidence that this key can redeem it.
The label never asserts that an output is change. There is still no payment
signing command, device-verified fee, or trusted input provenance.

`test-blue-payment-screen` checks full-hash equality, a one-byte mismatch,
missing account state, null output, P2SH with matching bytes, invalid type,
and the labels passed to the shared renderer. Clang 22.1.6 Debug with address
and undefined-behavior sanitizers passed 23/23 host tests; GCC 16.1.1
Release passed 23/23. Two independent builds with patched Blue SDK trees and
ARM GCC 16.2.0 produced identical `.text` SHA-256
`fb138d05d3c5c9a3b0850f02d00572779dafc8f2aab292d44bc54634c98a8abb`.
The image has 25,600 bytes of `.text`, 4,276 bytes of `.bss` including its
2,048-byte stack reservation, and zero initialized `.data`. The maximum
named stack path is 656 bytes, excluding BOLOS frames; 1,868 bytes remain
after `.bss` in the 6,144-byte SRAM region.

The 320×480 C23 renderer produced light and dark PNGs through:

```sh
test-blue-payment-screen /tmp/zcl-wallet-account-light.png \
  /tmp/zcl-wallet-account-dark.png
```

Their SHA-256 values were
`028c613a578429dc4afafe3426960fc0ac68df62ff9577679ea3ec1c126d2d97`
and `608c0fa5d5d836567fe835e493fa956de573c27735f7de0629b597ae3bc27416`,
respectively. Visual inspection found the address, amount, label, and both
buttons within the canvas. These renderings use SDK font bitmaps but do not
prove physical Blue pixels or touch behavior. Wallet 0.2.4 is not installed
or accepted by the installer.
