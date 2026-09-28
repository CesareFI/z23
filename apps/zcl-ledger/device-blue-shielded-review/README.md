<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# ZCL Shielded Review for Ledger Blue

Version 0.5.2 of this ISO C23 app is a read-only offline candidate for
Ledger Blue firmware 2.1.x. It accepts a complete Sapling-v4 transaction
in chunks of at most 220 bytes, uploaded six times. Each pass parses the wire independently and
derives one ZIP-243 section hash. A SHA-256 commitment to the first pass
rejects changed bytes in later passes. Only after all passes match does the
app return the ZIP-243 SIGHASH_ALL digest and public counts.

Tap NEXT / REFRESH during upload to redraw the current pass and byte count.
After finish, the screen shows public input/output and
Sapling spend/output counts, `PUB OUT` public output total, Sprout JoinSplit count,
and a digest prefix. NEXT / REFRESH shows the full digest in four rows. LARGER
TEXT and DARK are available; EXIT requests the Blue home screen. The screen
says `FEE UNKNOWN`, `SHIELDED HIDDEN; NO SIGNING`, and `READ ONLY; NO
SIGNING`. Neither the screen nor the digest is authorization to pay.

The protocol uses CLA `A5`, P1/P2 zero, and one-byte `Lc`:

| INS | Request | Response before `9000` |
| --- | --- | --- |
| `01` | Empty | `ZCL`, version `07`, review-only capability `40` |
| `20` | Four-byte wire length and four-byte branch ID, little endian | Empty |
| `21` | 1–220 wire bytes | Pass byte and four-byte received count |
| `22` | Empty, after a complete pass | Next pass byte |
| `23` | Empty, after pass six | 44-byte public summary and 32-byte ZIP-243 digest |
| `24` | Empty | Empty; erases the review |

Every rejected command erases pending or completed review state. The branch
ID is checked against known ZCL mainnet IDs but is not bound to a trusted
chain tip. Shielded recipients, amounts, memo contents, proofs, transparent
input values, change, and fee are not independently established by this
app. It has no key derivation, approval, or signing command.

Build with the reviewed C23 Blue SDK, its 2 KiB stack patch, Clang 22.1.6,
and ARM GCC 16.2.0:

```sh
make -C apps/zcl-ledger/device-blue-shielded-review \
  BOLOS_SDK=/path/to/blue-sdk \
  ARM_INCLUDE_DIR=/path/to/arm-none-eabi/include \
  GCCPATH=/path/to/toolchain/bin/ \
  CLANGPATH=/path/to/clang/bin/
```

The build rejects initialized `.data`, a non-Blue target, or a modeled
stack path that leaves less than 512 bytes of its 2,048-byte reserve. The
largest modeled path is the six-pass finish path. SDK and BOLOS frames are
outside this local stack model; physical USB and touchscreen behavior remain
unverified. The app is not installed on a Blue. The
[experiment record](../../../docs/experiments/2026-09-27-ledger-blue-signing-route.md)
contains measured image size, stack paths, and the image hash. A new build
must be independently checked before installation.

The host command can exercise the exact six-pass protocol against the C23
screen and APDU simulator, or against an open Ledger Blue HID device. Both
modes compare every progress reply, the final public facts, and the complete
ZIP-243 digest with Z23's independent host implementation. The simulator
does not reproduce BOLOS, USB timing, or the physical display. For a binary
Sapling-v4 transaction and a verified ZCL branch ID:

```sh
cmake -S apps/zcl-ledger -B build/blue-review \
  -DCMAKE_C_STANDARD=23 -DZCL_LEDGER_REVIEW_ONLY=ON
cmake --build build/blue-review --target zcl-blue-shielded-review
build/blue-review/zcl-blue-shielded-review \
  --simulate unsigned-v4.bin 0x76b809bb
```

Use `--blue /dev/hidrawN` in place of `--simulate` only after the exact
read-only image has been installed and the device app is open. Neither mode
signs, broadcasts, verifies shielded recipients or proofs, or establishes a
fee. The host command and review-only build do not require OpenSSL.

For visual inspection, the separate C23 screen simulator writes 41
320×480 PNG files: ready, start and end of each upload pass, summary and
digest in both themes, and every large-text detail in both themes. It uses
the same screen controller and layout constants as the device image, but
its host font renderer is an approximation of Blue BAGL glyphs.

```sh
cmake -S apps/zcl-ledger -B build/blue-visual
cmake --build build/blue-visual --target zcl-blue-shielded-screen-sim
build/blue-visual/zcl-blue-shielded-screen-sim \
  unsigned-v4.bin 0x76b809bb /tmp/zcl-blue-review
```

The PNGs are review artifacts, not evidence that the physical display or
touch events work. The command does not connect to the Blue.

## Sapling key compatibility

This review app has no Sapling keys. Z23's current software Sapling wallet
uses an independent 32-byte seed and ZIP-32 path `m/32'/147'/account'`.
ZIP-32 starts from the wallet seed with personalized BLAKE2b-512. The Blue
2.1.x app API exposes BIP-32 derivation, including a custom HMAC seed-key
variant, but no raw wallet seed or ZIP-32 derivation call. A Blue-derived
Sapling account therefore cannot be presented as the same account as Z23's
existing software wallet. A separate derivation scheme would need a
domain-separated seed source, a public recovery specification, independent
C23 recovery tests, and explicit account identity in the wallet UI before
receiving funds or enabling signing. The
[ZIP-32 specification](https://zips.z.cash/zip-0032),
[Blue SDK interface](https://github.com/LedgerHQ/blue-secure-sdk/blob/master/include/os.h),
and [Zcash Ledger security audit](https://zecsec.com/audits/zcash-ledger-audit-report-v2.pdf)
document the compatibility and recovery risks.
