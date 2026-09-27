<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# ZCL Review for Ledger Blue

This C23 app accepts up to 3,712 bytes of a raw ZCL Sapling-v4 transaction
over USB and returns a structural summary and SHA-256 digest of the exact
transaction bytes. It counts transparent inputs and
outputs, Sapling spends and outputs, and Sprout JoinSplits. It also reports
the public output total, value balance, lock time, and expiry height. The
touchscreen starts with `CONNECT Z23`, `SEND A TRANSACTION`, and `TAP NEXT PAGE
TO VIEW`. After a successful review, tap
`NEXT PAGE` to see the public output total, Sapling and Sprout counts, and
the first eight bytes of the transaction SHA-256 digest. Further taps show
each transparent output's amount in ZCL and, for P2PKH or P2SH, its independently
derived ZCL mainnet address. Other script pages show the script length and
the first ten bytes of its SHA-256 digest. OP_RETURN pages say `TOKEN STATUS
UNVERIFIED`; a token marker alone is not token validation. After the final
output, NEXT PAGE returns to the summary. `EXIT` returns to
the Blue home screen. The app has no key derivation, approval, or signing
command. Given an explicit consensus branch ID, it also computes the ZIP-243
shielded SIGHASH_ALL digest. Its screen does not display recipients,
shielded recipients, shielded amounts, or a verified fee. Its response is not user
authorization of a payment.

Build with the reviewed Blue SDK and an ISO C23 compiler:

```sh
make -C apps/zcl-ledger/device-blue-review \
  BOLOS_SDK=/path/to/blue-sdk \
  ARM_INCLUDE_DIR=/path/to/arm-none-eabi/include \
  GCCPATH=/path/to/toolchain/bin/ \
  CLANGPATH=/path/to/clang/bin/
```

The build checks for an empty `.data` section. Extract the 29,952-byte code
image and check its SHA-256 before installing:

```sh
llvm-objcopy -O binary --only-section=.text \
  apps/zcl-ledger/device-blue-review/bin/app.elf /tmp/zcl-review.bin
sha256sum /tmp/zcl-review.bin
```

Version 0.4.0's former image hash was
`f442caa2e21e3b2f830f48f71ba23531ba6cfdf51bd4d888ee59bfd0e0e72dae`.
It is no longer accepted by the installer after a live USB lockup.
The measured `.bss`, including the reserved stack, is 6,024 bytes. The
3,712-byte transaction limit leaves room for the reply cache and screen
strings within the Blue's SRAM.
Install only on the dedicated test Blue at its home screen using
`zcl-blue-install /dev/hidrawN --ca-install CA_KEY_FILE /tmp/zcl-review.bin`.
Delete an older ZCL Review app first; the Blue rejected installation over
an existing icon with status `6a80` at commit.
If the signed install is rejected, the unsigned install command is
`zcl-blue-install /dev/hidrawN /tmp/zcl-review.bin`; Blue will show its
non-genuine application warning when the app opens. EXIT returns to home.
Remove it from home using `zcl-blue-install /dev/hidrawN --ca-delete-review
CA_KEY_FILE` for a signed install, or `--delete-review` for an unsigned one.

The host's `zcl-tx-review`
command can compare the app's reply to its own parser using
`--blue /dev/hidrawN`, once a separately reviewed image is installed and
open. The earlier version 0.1.0 signed image installed on the dedicated Blue running BOLOS 2.1.1,
and a synthetic one-spend, one-output Sapling fixture returned a matching
summary and exact-byte digest. The owner confirmed the signed app opened
without BOLOS's non-genuine warning and exited normally. Version 0.2.0
received a successful install response after the previous Review icon was
deleted, but its icon is absent from the owner's home screen. Installation,
touchscreen operation, and live ZIP-243 behavior remain unverified. Version 0.4.0
installed and opened, but a 245-byte test-vector review stopped USB replies
and EXIT did not respond. The owner restarted the Blue and Z23 deleted the
app. Do not install version 0.4.0. Running
the host command without `--blue` only parses a local file.

Protocol commands use CLA `A5`, P1/P2 zero, and one-byte `Lc`:

| INS | Request | Successful response before `9000` |
| --- | --- | --- |
| `01` | Empty | `ZCL`, protocol version `06`, review-only capability `40` |
| `10` | Two-byte little-endian transaction length | Empty |
| `11` | Transaction chunk | Empty |
| `12` | Empty | 44-byte little-endian structural summary, then 32-byte SHA-256 digest |
| `13` | Empty | Empty; clears pending review |
| `14` | Four-byte little-endian consensus branch ID | 32-byte ZIP-243 shielded SIGHASH_ALL digest |

Each chunk is at most 220 bytes from the host CLI. An invalid size or
truncated transaction fails. `12` consumes the pending review even if the
transaction is invalid. `14` requires a complete transaction and leaves it
pending for `12`; the caller must supply a branch ID valid for the transaction's
height. The app does not check that relationship. Neither digest is a device
approval. The summary fields and limitations are documented in
the [host guide](../README.md).
