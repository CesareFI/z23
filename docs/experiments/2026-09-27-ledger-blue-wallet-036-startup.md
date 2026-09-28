<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Ledger Blue Wallet 0.3.6 startup shell

Date: 2026-09-27T22:13:29-04:00 (2026-09-28T02:13:29Z).

The previous host UI test compiled the payment controller but never compiled
the Blue Wallet's `main.c`. The new C23 startup test compiles that source
directly against a bounded host SDK shell. It executes the startup sequence
until the first APDU receive boundary, then exercises the displayed EXIT
callback through `io_event`. Its SHA-256 and RIPEMD-160 calls use Z23's C23
implementations; its BOLOS BIP32 and EC public-key calls return deterministic
test points. The resulting 35-character receive address is checked against
the host's independently formatted address from the captured HASH160.

The shell covers successful startup, failure to validate the PIN, malformed
private-key initialization, public-key generation failure, and failure of
the second derivation path. It also exercises USB reset, USB suspend, and
the approval-timeout display event. The second-path failure initially failed
the test: `main.c` wiped its public key and whole address but retained the
three formatted address lines. Wallet 0.3.6 clears those lines before showing
`ADDRESS UNAVAILABLE`; the test then passed. The boot workspace is zero after
each simulated derivation failure, and EXIT returns through its event callback.

Two independent builds from Blue SDK commit
`3c710b4c62ad847599a2deb0932a50dd1ae4bdff` with patch SHA-256
`4919fd81ba7a3a80880065aaf7898c2edd603b2586f6086a88570c73996019f6`
produced byte-identical 40,960-byte `.text` images, SHA-256
`435b6f03a62e99daa668b85c71e895f7b64ad7a056b575367c5bf2af460e09c3`.
The ELF has zero `.data` and 5,120 bytes of `.bss`, including the 2,048-byte
reserved stack. The 1,024 bytes above `.bss` meet the current SRAM gate.
The full Ledger CTest suite passed 50/50 on Clang 22.1.6 Debug with
ASan/UBSan and 50/50 on GCC 16.1.1 Release. A local installer invocation
with the exact candidate image, a nonexistent HID path, and no Blue attached
rejected the image hash before opening USB.
The staged C23 cyclomatic gate selftest passed, then scanned 63,018
functions in 4,542 files at cap 15 with 4,112 exact baseline pins.

The shell replaces neither BOLOS nor its USB and touchscreen firmware. Its
fake public points test control flow and address formatting, not real BIP32
or EC behavior. The APDU loop stops at its first receive call; transaction
upload, signing, power loss, USB descriptor recovery, firmware stack frames,
and display timing still require other tests. Wallet 0.3.6 remains an offline
candidate and has not been installed on the physical Blue.
