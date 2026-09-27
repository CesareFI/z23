<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# ZCL Wallet receive candidate for Ledger Blue

This C23 app is an offline-tested transparent receive candidate. It derives
`m/44'/147'/0'/0/0` on the Blue after PIN validation, retains only the
compressed public key, and displays its ZCL mainnet P2PKH address across
three large-text lines. The host reads the public key through INS `02`,
validates that point on secp256k1, and independently computes the address.
The owner must compare all 35 characters with the Blue display before using
the address. EXIT returns to the home screen.

The app has no transaction, signature, private-key export, path selection,
Sapling, multisig, or token command. Opening the app derives the fixed key;
USB commands cannot trigger derivation or redraw its screen. Its offline
preview is an approximation of BAGL rendering. The image has not been
installed or tested on physical hardware, so no receiving funds are
recommended from this build.

## Build

Use the reviewed Blue 2.1.x SDK and an ISO C23 Clang/ARM toolchain:

```sh
make -C apps/zcl-ledger/device-blue-wallet \
  BOLOS_SDK=/path/to/blue-sdk \
  ARM_INCLUDE_DIR=/path/to/arm-none-eabi/include \
  GCCPATH=/path/to/toolchain/bin/ \
  CLANGPATH=/path/to/clang/bin/
arm-none-eabi-objcopy -O binary --only-section=.text \
  apps/zcl-ledger/device-blue-wallet/bin/app.elf /tmp/zcl-wallet.bin
sha256sum /tmp/zcl-wallet.bin
```

The build rejects initialized `.data`, keeps at least 512 bytes of Blue app
SRAM outside the 2,048-byte stack, and checks named derivation, address,
APDU, and screen-event stack paths with a separate 512-byte stack margin.
The linked 0.1.0 image uses 3,348 bytes of `.bss` and leaves 748 bytes of
the 6,144-byte app SRAM region outside the stack. The stack gate cannot
measure BOLOS firmware frames. The C23
installer accepts only the
exact 14,848-byte image with SHA-256
`baf36150563cecd659692434800d5bb106a9679a9fa6e36598a3e38fb0836df2`.
Its BOLOS metadata grants the fixed ZCL path needed for derivation. After
independent code review, test install, open, screen, USB, EXIT, and recovery
on the dedicated Blue before promoting this candidate. The owner-controlled
CA install command is:

```sh
zcl-blue-install /dev/hidrawN --ca-install CA_KEY_FILE /tmp/zcl-wallet.bin
```

## USB protocol

All APDUs use CLA `A5`, P1/P2 zero, and empty `Lc`.

| INS | Reply before `9000` |
| --- | --- |
| `01` | `ZCL`, protocol version `08`, receive-only capability `01` |
| `02` | 33-byte compressed public key when the address is ready |

INS `02` returns `6985` if derivation or address formatting fails. No
command signs or approves a payment. After hardware validation, run
`zcl-ledger receive-address --json /dev/hidrawN` while the app is open and
compare the returned address with all characters on the Blue screen.
