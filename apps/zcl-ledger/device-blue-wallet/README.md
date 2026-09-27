<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# ZCL Wallet receive and read-only review candidate for Ledger Blue

Version 0.2.0 derives
`m/44'/147'/0'/0/0` on the Blue after PIN validation, retains only the
compressed public key, and displays its ZCL mainnet P2PKH address across
three large-text lines. The host reads the public key through INS `02`,
validates that point on secp256k1, and independently computes the address.
The owner must compare all 35 characters with the Blue display before using
the address. EXIT returns to the home screen.

The same app now has read-only transaction review commands. It accepts an
unsigned, all-transparent v4 transaction in three complete passes, pauses
at each P2PKH or P2SH output, displays the exact amount and all 35 address
characters, and requires a touchscreen CONTINUE tap before the next chunk.
EXIT cancels and returns home. There is no USB acknowledgement, payment
signature, private-key export, path selection, Sapling spend, multisig, or
token command. The supplied branch ID and spent output are not authenticated;
the replay digest is discarded. The image has not been installed or tested
on physical hardware. Its BAGL preview and host tests do not prove Blue
touch, USB, or display behavior, so no funds should be received with it yet.

## Build

Use Ledger's open-source Blue SDK at revision
`3c710b4c62ad847599a2deb0932a50dd1ae4bdff` with the reviewed
[`C23 SDK patch`](../toolchain/blue-secure-sdk-2.1-c23.patch). Apply that
patch to a clean SDK checkout. Use Clang 22.1.6 and ARM GCC 16.2.0:

```sh
git -C /path/to/blue-secure-sdk checkout 3c710b4c62ad847599a2deb0932a50dd1ae4bdff
git -C /path/to/blue-secure-sdk apply /path/to/z23/apps/zcl-ledger/toolchain/blue-secure-sdk-2.1-c23.patch
make -C apps/zcl-ledger/device-blue-wallet \
  BOLOS_SDK=/path/to/blue-sdk \
  ARM_INCLUDE_DIR=/path/to/arm-none-eabi/include \
  GCCPATH=/path/to/toolchain/bin/ \
  CLANGPATH=/path/to/clang/bin/
arm-none-eabi-objcopy -O binary --only-section=.text \
  apps/zcl-ledger/device-blue-wallet/bin/app.elf /tmp/zcl-wallet.bin
sha256sum /tmp/zcl-wallet.bin
```

The build rejects initialized `.data`, keeps at least 512 bytes of app SRAM
after `.bss`, and checks named derivation, upload, formatting, replay, and
touch paths against the 2,048-byte stack reservation with a separate
512-byte margin. The linked 0.2.0 image has 24,832 bytes of `.text`, 4,236
bytes of `.bss`, and zero `.data`. Its `.bss` includes the linker-reserved
stack; 1,908 bytes remain after that section in the 6,144-byte app SRAM
region. The largest named C path sums to 648 bytes, excluding BOLOS firmware
frames. Two clean builds, using the original patched SDK and a fresh SDK
checkout with the repository patch applied, produced the same image SHA-256:
`97db7a9ab725b03fd55365a057a552488f88f106ab8cd957bbda0faad1875113`.
The installer accepts only byte-pinned images. Its BOLOS metadata grants the
fixed ZCL derivation path. Physical install, open, USB, touchscreen, EXIT,
and recovery checks are still required for this exact image.

The owner-controlled CA install command, after those checks are planned, is:

```sh
zcl-blue-install /dev/hidrawN --ca-install CA_KEY_FILE /tmp/zcl-wallet.bin
```

## USB protocol

All APDUs use CLA `A5`, P1/P2 zero, and an exact one-byte `Lc`.

| INS | Reply before `9000` |
| --- | --- |
| `01` | `ZCL`, protocol version `09`, receive and review capability `03` |
| `02` | 33-byte compressed public key when the address is ready |
| `20` | Begin read-only replay: 12-byte length, input index, branch ID |
| `21` | Feed one chunk; reply reports pass and pending output |
| `22` | Advance replay pass; reply reports pass and output count |
| `23` | Finish complete replay; reply reports output count |
| `24` | Cancel review |
| `25` | Query six nonsecret review-state bytes |

INS `02` returns `6985` if derivation or address formatting fails. A review
upload chunk must stop on the exact output boundary. Only the touchscreen
CONTINUE callback acknowledges that output; USB cannot do so. Any malformed
command invalidates the review. USB reset or suspend also cancels an idle
review and returns to the receive screen. No command signs or approves a payment.
After hardware validation, run
`zcl-ledger receive-address --json /dev/hidrawN` while the app is open and
compare the returned address with all characters on the Blue screen.
