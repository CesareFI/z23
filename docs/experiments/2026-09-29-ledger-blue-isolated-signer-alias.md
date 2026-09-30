<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Isolated Sapling signer output and input separation

## Intent

Keep the RedJubjub signature output separate from the protected scalar,
fresh entropy, and reviewed transaction digest. The isolated signer is a
candidate only; no Wallet APDU invokes it or grants device key access.

## Finding and correction

The previous equation test required successful signing when the 64-byte
signature output started at the 32-byte secret input. That permitted the
signer to replace its caller-owned secret. A rejected call could also wipe
an aliased input through the signature buffer. The signer now checks all
three output/input ranges before hashing or writing. An overlap returns
false with the caller's storage preserved. Rejections with a separate
output buffer retain the existing output-clearing behavior.

The host equation test checks exact and partial secret aliases, a partial
entropy alias, and a partial digest alias, including full preservation of
the surrounding storage. The original fixed signature and public equation
vectors still pass, as does a subsequent independent signing call. The
Cortex-M3 and Cortex-M0 emulator tests still pass with the same mapped-key
fixture. Their reported maximum stack use is 1,452 and 1,536 bytes of their
2,048-byte test reservations, respectively.
Release and sanitized Debug each passed 62/62 tests. All 33 fast lint gates,
the 554-file/80-section consensus-core seal, inline documentation paths,
bound documentation claims, and local Markdown links passed.

## Scope

The change affects the isolated signer and its host and ARM emulator
fixtures. It does not alter a Blue Wallet or Shielded Review image. The
device-derived signer remains compile-blocked until target side-channel
validation; emulator correctness and stack measurements cannot establish
physical-device timing or spending safety.
