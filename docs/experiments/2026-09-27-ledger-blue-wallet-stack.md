<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Blue receive image stack and substitution gates

Local time: 2026-09-27T00:15:17-04:00

UTC: 2026-09-27T04:15:17Z

## Question

Does the Wallet build reject a too-small stack reservation, and does the C23
installer reject a changed image before connecting to the Blue?

## Evidence

The shared C23 stack checker now requires Wallet frame records for `main`,
`zcl_base58_encode`, `blue_wallet_receive_split`, `answer_command`,
`blue_wallet_handle`, and `io_event`. Against the reviewed SDK's 2,048-byte
reserve, the measured call-chain sums were 392 bytes for derivation and
formatting, 312 for layout, 88 for APDU handling, and 32 for an event. The
build retains a 512-byte margin. A copied linker script with an 800-byte
stack reservation was rejected. The Review app's existing check still passed
with 768-byte APDU and 928-byte screen paths.

The unchanged Wallet `.text` image remained 14,848 bytes with SHA-256
`baf36150563cecd659692434800d5bb106a9679a9fa6e36598a3e38fb0836df2`.
The C23 installer accepted this image through its hash gate before rejecting
`/dev/null` as a non-Blue device. A one-byte modification at offset 100 was
rejected at the hash gate before any device open. The Blue was connected and
answered read-only BOLOS app-info (`2.1.1`) on `/dev/hidraw1`; no install
or deletion command was sent.

These measured frames exclude BOLOS firmware calls and asynchronous stack
effects. On-device opening, touchscreen EXIT, USB transfer, restart recovery,
and address comparison remain required before funds are received.
