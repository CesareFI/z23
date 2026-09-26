<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Ledger Blue read-only transaction review screen

Recorded: 2026-09-26T19:38:21-04:00 (2026-09-26T23:38:21Z)

The dedicated Blue reported BOLOS 2.1.1 at its home screen. The pinned
ZCL Review 0.2.0 image (19,968 bytes, SHA-256
`9de444a3179520d7edf6a60e67417ef5e57942eb3d04d53e73fa1a7f69ad7528`)
was rebuilt from the unchanged app sources with Clang 22 and a locally
extracted, signed GNU Arm 16.2 toolchain; its bytes matched the previous
pin. An installation attempt over an existing ZCL Review icon reached the
commit command and returned `6a80`. After Z23 deleted the old Review app
through the owner CA, the Blue returned a successful signed install response
for the same 0.2.0 image. The owner reported that its icon is absent from
the home screen. Installation, touchscreen operation, and ZIP-243 USB
behavior remain unverified.
At 2026-09-26T19:47:04-04:00 (2026-09-26T23:47:04Z), a read-only
`zcl-ledger app-info --json /dev/hidraw1` returned BOLOS 2.1.1. The app
was not open, so no transaction was sent to the Blue.

ZCL Review 0.3.0 adds a read-only screen refreshed by its VIEW LATEST
button. The screen formats the independently parsed public input/output
counts, total public output value, Sapling spend/output counts, Sprout count,
and first eight bytes of the exact transaction SHA-256. It explicitly says
`NO SIGNING; SHIELDED HIDDEN`. The host CLI now reports the full transaction
SHA-256 so the prefix can be compared. The screen has no approval control,
no key access, and no signing APDU.

The Blue's static RAM limit required independent screen storage. The maximum
wire transaction length is 3,840 bytes in 0.3.0; six 32-byte screen lines
use a separate 192-byte buffer. The measured image is 23,296 bytes of
`.text`, zero `.data`, and 6,068 bytes of `.bss` including the reserved
stack; its SHA-256 is
`ae5755690d8317fda9fa4c827a8c5cdc60497b0cd92a196fe4e28e69b62f9eaf`.
The installer allowlist pins those bytes. Clang Debug with sanitizers and
GCC Release each passed 11 of 11 local tests, including a protocol-to-screen
SHA prefix check. A clean Arm build reproduced the same 23,296-byte image
byte for byte. Version 0.3.0 has not been installed or physically tested.
The repository cyclomatic complexity gate passed with a cap of 15 after
splitting the host CLI output functions.

The screen cannot show encrypted Sapling recipient or amount data, individual
transparent recipients, input ownership, or a verified fee. The next signing
app must provide those facts and bind a device approval to the exact
transaction before it can safely sign a payment. Larger transactions need
bounded streaming instead of silently truncating them.
