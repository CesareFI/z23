<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Ledger Blue signing footprint

Local time: 2026-09-27T05:59:15-04:00

UTC: 2026-09-27T09:59:15Z

## Question

Do the compiled transparent signing boundary and Blue SDK signer fit the
device image and app stack budgets before a signing APDU is introduced?

## Method

The ordinary read-only Wallet 0.2.14 image was compared with a build that
appended `-Wl,-u,blue_wallet_sign_digest` and
`-Wl,-u,blue_payment_sign_next` to the SDK's existing linker flags. The
forced symbols make the signer and its dependencies appear in the ELF but
do not create a USB command. The build ran `check-image` and `check-stack`
against two independently patched Blue SDK trees at revision
`3c710b4c62ad847599a2deb0932a50dd1ae4bdff`.

The C23 stack checker now requires the compiled frame data for the signing
boundary, digest extraction, DER normalization, and public-key hash. It
adds each possible callee to the current `answer_command` →
`wallet_payment_command` → `blue_payment_sign_next` prefix. A negative
check replaced the measured signer frame of 112 bytes with 1,600 bytes in
a temporary copy of its `.su` file; the gate had to refuse the image.

## Result

The ordinary image has 33,792 bytes of `.text`, 5,472 bytes of `.bss`, and
zero `.data`. The forced-link image has 37,120 bytes of `.text`, 5,472 bytes
of `.bss`, and zero `.data`. Its `.text` SHA-256 is
`cb0501decf4813cc99a505948a641f6b376099d040a10f3ca9601363d068937a`
in both SDK builds. The four candidate named C paths sum to 640 bytes
(device signer), 552 bytes (digest extraction), 624 bytes (DER), and 680
bytes (public hash). The existing read-only maximum is 752 bytes. The
reserve is 2,048 bytes, and the gate retains a 512-byte margin. With the
temporary 1,600-byte signer frame, its candidate path became 2,128 bytes
and the gate rejected it.

Compiler: Clang 22.1.6 for ARM app C, ARM GCC 16.2.0 for linking, and
Clang 22.1.6 for the C23 stack checker. CPU: AMD Ryzen 7 PRO 8840U with
Radeon 780M Graphics. Test date: 2026-09-27.

## Limit

The forced-link build is a size experiment, not an installable payment
signer. The callback is unreachable over USB. These stack sums exclude
BOLOS syscall and event-handler frames, and the current wallet command
frame may change when signing dispatch is connected. Physical device
signing, active-path stack use, interruption recovery, independent signature
verification, and chain-backed input status remain untested.
