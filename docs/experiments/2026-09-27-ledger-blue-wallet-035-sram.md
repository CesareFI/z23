<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Ledger Blue Wallet 0.3.5 SRAM reuse

Date: 2026-09-27T22:01:01-04:00 (2026-09-28T02:01:01Z).

Wallet 0.3.4 froze on physical startup and was deleted. Its exact linked
image had 5,476 bytes of `.bss`, including a 2,048-byte reserved stack,
leaving 668 bytes between the end of `.bss` and the top of the Blue's 6 KiB
SRAM region. The cause of the freeze is unproven. The stricter 1,024-byte
post-`.bss` gate rejects that image.

Wallet 0.3.5 keeps the 16-input transparent transaction limit. Output-screen
text and the previous-transaction parser now occupy one union: the parser
can start only after all output pages are acknowledged and the transaction
review is complete. Startup private and public key material now occupies a
union with payment state: the app wipes that material before processing any
payment APDU. Three amount labels have 24-byte buffers, enough for the
maximum allowed value `21000000.00000000 ZCL` plus the terminating byte;
the 16-byte path label covers the longest current path text.

The pinned Blue SDK commit was
`3c710b4c62ad847599a2deb0932a50dd1ae4bdff` with reviewed patch
SHA-256 `4919fd81ba7a3a80880065aaf7898c2edd603b2586f6086a88570c73996019f6`.
Two independently patched SDK trees produced byte-identical 40,960-byte
`.text` images, SHA-256
`d99812f8ed00e4accc3466efe1eb400b756a38677a84175897785a031e2db448`.
The ARM link has zero `.data` and 5,120 bytes of `.bss`, including the
2,048-byte stack reservation. The 1,024 bytes above `.bss` extend the
downward-growing stack from the SDK's `_estack` at the top of SRAM; they are
not an independent heap. The named stack-path gate passed with a maximum
752-byte payment-upload path and 736-byte public-hash signing path. Those
figures exclude BOLOS firmware frames.

The C23 Ledger host suite passed 49/49 with Clang 22.1.6 Debug and 49/49
with GCC 16.1.1 Release. The boot-workspace test checks its wipe bounds and
the amount formatter checks a guarded 24-byte buffer at the maximum ZCL
value. The C23 cyclomatic checker selftest passed, then scanned 62,986
functions in 4,541 files at cap 15 with 4,112 exact baseline pins. The
repository-wide `make check-cyclomatic-complexity` wrapper was stopped after
it began rebuilding unrelated vendor libraries; the direct checked-in
`z23-lint` gate binary from the Blue output worktree produced the result.

Wallet 0.3.5 is an offline candidate. It has not been installed, opened, or
run through USB and touch on the physical Blue. The host simulator does not
execute BOLOS startup, firmware stack frames, real USB events, or touch.
The installer does not permit this image, so the previous physical freeze
cannot be repeated by the normal install command. A physical startup and
exit exercise remains necessary before payment signing tests.
