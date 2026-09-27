<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# ZCL Wallet receive and read-only review candidate for Ledger Blue

Version 0.2.5 derives
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
EXIT cancels and returns home. After the three-pass review, Z23 uploads each
complete previous transaction in input order. The app checks SHA-256d against
its captured outpoint, selects the indexed P2PKH output, derives all input
amounts, and displays the fee calculated from those inputs and the reviewed
outputs. The Blue does not verify chain inclusion, UTXO status, maturity, or
ownership. There is no USB output acknowledgement, payment signature,
private-key export, path selection, Sapling spend, multisig, or token command.
The supplied branch ID remains unauthenticated; the replay digest is
discarded. Version 0.2.1 reached the Blue: its receive
screen and EXIT worked, and all 35 address characters matched the host result
`t1RAmKL4KFauUXGswvMvk66aS5UL33ck1Uz`. A synthetic transaction review
then stopped USB replies and left EXIT unresponsive. The owner restarted the
Blue; Z23 deleted Wallet, Sign Test, and Probe and verified an empty catalog.
The 0.2.0 and 0.2.1 review images are excluded from the installer. Version
0.2.2 deferred screen redraw until after the APDU reply. Version 0.2.3 also
keeps the transmit and redraw outside the request exception handler, so a
post-reply display exception cannot schedule a second response. A receive
exception before a complete APDU now unwinds to the outer app handler.
Version 0.2.4 labels an exact P2PKH hash match to the
Blue-derived fixed account as “THIS ACCOUNT,” other P2PKH outputs as “OTHER
ADDRESS,” and P2SH outputs as “P2SH ADDRESS.” It does not infer ownership of
P2SH or call an output change without verified inputs and account context.
Version 0.2.5 remains uninstalled. Do not receive funds or sign payments with it.

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
512-byte margin. The linked 0.2.5 image has 29,952 bytes of `.text`, 5,084
bytes of `.bss`, and zero `.data`. Its `.bss` includes the linker-reserved
stack; 1,060 bytes remain after that section in the 6,144-byte app SRAM
region. The largest named C path sums to 672 bytes, excluding BOLOS firmware
frames. Two independent builds using patched SDK trees produced image SHA-256
`6c484545832c006cd620401e4e0c14f56f1a88550ccad9c440b988dc8c333a16`.
The installer does not accept this image yet. Device-side USB, screen, EXIT,
and recovery checks are pending.

## USB protocol

All APDUs use CLA `A5`, P1/P2 zero, and an exact one-byte `Lc`.

| INS | Reply before `9000` |
| --- | --- |
| `01` | `ZCL`, protocol version `0A`, receive, review, and previous-wire capability `07` |
| `02` | 33-byte compressed public key when the address is ready |
| `20` | Begin read-only replay: 12-byte length, input index, branch ID |
| `21` | Feed one chunk; reply reports pass and pending output |
| `22` | Advance replay pass; reply reports pass and output count |
| `23` | Finish complete replay; reply reports output count |
| `24` | Cancel review |
| `25` | Query six nonsecret review-state bytes |
| `26` | Begin the next previous wire with a four-byte little-endian length |
| `27` | Feed previous-wire bytes; exact SHA-256d and structure are checked at finish |
| `28` | Finish the previous wire; reply contains bound count, input count, fee-ready flag, and eight-byte fee |

INS `02` returns `6985` if derivation or address formatting fails. A review
upload chunk must stop on the exact output boundary. Only the touchscreen
CONTINUE callback acknowledges that output; USB cannot do so. Previous-wire
commands are accepted only after all outputs and the complete spending wire
have been reviewed. Any malformed command invalidates the review. USB reset
or suspend also cancels an idle review and returns to the receive screen. No
command signs or approves a payment.
After hardware validation, run
`zcl-ledger receive-address --json /dev/hidrawN` while the app is open and
compare the returned address with all characters on the Blue screen.
