<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Ledger Blue current-source image rebuild

Date: 2026-09-29T07:55:15-04:00 (2026-09-29T11:55:15Z).

## Question

Do the pinned Blue Wallet and read-only Shielded Review image bytes still
match their reviewed pins after the host-side test and documentation changes?

## Method

The source was local signed commit `51ba8f2b751ef30ac5be200e11e9e6422ec2ace6`.
The open-source Blue SDK was commit
`3c710b4c62ad847599a2deb0932a50dd1ae4bdff` with reviewed patch-diff
SHA-256 `4919fd81ba7a3a80880065aaf7898c2edd603b2586f6086a88570c73996019f6`.
Clang 22.1.6 compiled C23; arm-none-eabi-gcc 16.2.0 assembled and linked.
The host CPU was an AMD Ryzen 7 PRO 8840U with Radeon 780M Graphics.
Both device build directories were cleaned before rebuilding with their
image and stack gates. The commands were:

```sh
make -C apps/zcl-ledger/device-blue-wallet BOLOS_SDK=/tmp/z23-blue-sdk-repro-20260927 ARM_INCLUDE_DIR=/tmp/z23-arm-toolchain/usr/arm-none-eabi/include GCCPATH=/tmp/z23-arm-toolchain/usr/bin/ CLANGPATH=/usr/bin/ clean
make -C apps/zcl-ledger/device-blue-wallet BOLOS_SDK=/tmp/z23-blue-sdk-repro-20260927 ARM_INCLUDE_DIR=/tmp/z23-arm-toolchain/usr/arm-none-eabi/include GCCPATH=/tmp/z23-arm-toolchain/usr/bin/ CLANGPATH=/usr/bin/ -j2
make -C apps/zcl-ledger/device-blue-shielded-review BOLOS_SDK=/tmp/z23-blue-sdk-repro-20260927 ARM_INCLUDE_DIR=/tmp/z23-arm-toolchain/usr/arm-none-eabi/include GCCPATH=/tmp/z23-arm-toolchain/usr/bin/ CLANGPATH=/usr/bin/ clean
make -C apps/zcl-ledger/device-blue-shielded-review BOLOS_SDK=/tmp/z23-blue-sdk-repro-20260927 ARM_INCLUDE_DIR=/tmp/z23-arm-toolchain/usr/arm-none-eabi/include GCCPATH=/tmp/z23-arm-toolchain/usr/bin/ CLANGPATH=/usr/bin/ -j2
```

## Result and boundary

Wallet 0.3.44 rebuilt with `.text` SHA-256
`d5288c8cbc2e1e5fc2f47d6d11d14796744edf3346effd5f7eb937d65e4461a8`
and Intel HEX SHA-256
`dd0b0cf0f72c1d2797941f16f198a479a6a57a63d260770f5a9f54c09e051c86`.
It uses 53,504 bytes of `.text`, zero `.data`, and 5,120 bytes of `.bss`.
The largest modeled payment path uses 1,128 bytes; the payment finish path
uses 1,112 bytes, each with the required 512-byte margin inside the
2,048-byte reserved stack. BOLOS frames are excluded.

Shielded Review 0.5.11 rebuilt with `.text` SHA-256
`671d323a0d9720bdd79af3cbb53166481da04952d7101917ced5192dba1fa4ff`
and Intel HEX SHA-256
`9434836d43c109bcc7d42ef7af2046f59239e34cadba7ce656d538be6b61e384`.
It uses 34,048 bytes of `.text`, zero `.data`, and 4,360 bytes of `.bss`.
Its largest modeled stack path uses 888 bytes of its 1,536-byte reservation;
BOLOS frames are excluded.

The offline installer recognized the Wallet image as a signing path and
Shielded Review as a read-only path. It blocked both installations pending
physical validation. This rebuild used one patched SDK checkout and matches
the earlier two-copy image pins; it does not establish independent review,
physical open or touch behavior, or permission to install.
