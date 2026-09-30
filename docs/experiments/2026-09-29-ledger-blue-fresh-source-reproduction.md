<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->
# Ledger Blue fresh-source image reproduction

## Question

Do Wallet 0.3.40 and Shielded Review 0.5.7 reproduce their recorded ARM
images when built from a fresh checkout of the signed local batch?

## Method

At 2026-09-29T03:01:13-04:00 (2026-09-29T07:01:13+00:00), clone commit
`70346798ea55fd9e1163183577562555bae10a79` with `git clone
--no-hardlinks` into a new source directory. Build the two apps in distinct
directories with separate copies of the pinned Blue SDK. Each SDK copy was
at commit `3c710b4c62ad847599a2deb0932a50dd1ae4bdff` and had the required
working-tree patch SHA-256
`4919fd81ba7a3a80880065aaf7898c2edd603b2586f6086a88570c73996019f6`.
The app Makefiles checked both identifiers before compilation.

The host was an AMD Ryzen 7 PRO 8840U with Clang 22.1.6. The ARM compiler
and binutils were in `/tmp/z23-arm-toolchain/usr/bin/`.

```sh
git clone --no-hardlinks /tmp/z23-blue-standalone-20260928 /tmp/z23-blue-independent-20260929
git -C /tmp/z23-blue-independent-20260929 checkout 70346798ea55fd9e1163183577562555bae10a79
make -C /tmp/z23-blue-independent-20260929/apps/zcl-ledger/device-blue-wallet \
  BOLOS_SDK=/tmp/z23-blue-sdk-repro-20260927 \
  ARM_INCLUDE_DIR=/tmp/z23-arm-toolchain/usr/arm-none-eabi/include \
  GCCPATH=/tmp/z23-arm-toolchain/usr/bin/ CLANGPATH=/usr/bin/ -j2
make -C /tmp/z23-blue-independent-20260929/apps/zcl-ledger/device-blue-shielded-review \
  BOLOS_SDK=/tmp/z23-blue-sdk-repro-independent-20260927 \
  ARM_INCLUDE_DIR=/tmp/z23-arm-toolchain/usr/arm-none-eabi/include \
  GCCPATH=/tmp/z23-arm-toolchain/usr/bin/ CLANGPATH=/usr/bin/ -j2
```

Extract `.text` with `arm-none-eabi-objcopy -O binary -j .text`; hash the
extracted bytes and each `bin/app.hex` with `sha256sum`. Measure sections
with `arm-none-eabi-size -A bin/app.elf`.

| App | `.text` bytes | `.data` bytes | `.bss` bytes | `.text` SHA-256 | HEX SHA-256 |
| --- | ---: | ---: | ---: | --- | --- |
| Wallet 0.3.40 | 51,712 | 0 | 5,120 | `6b4eed2f4c8c2f61206de3e269c26c9eb6ee00a4939200768694f27c761dc94b` | `23d9d029473dc8faf4b656cfc2023696e1f2b21e82c76b92821bfb882cf593b8` |
| Shielded Review 0.5.7 | 33,280 | 0 | 4,320 | `542f9d5e95e1160c56ddcb45207f31a9381f54d14d0dc2daa2e331a60e54a728` | `e6f7b4a90063342d41d9807c2339a6432257c361e17638d581a8e90e74ac92c2` |

The Wallet stack checker reported an 872-byte modeled approval touch path
within its 2,048-byte reservation and a required 512-byte margin. The
Shielded Review checker reported an 840-byte maximum modeled path within
its 1,536-byte reservation. These models exclude BOLOS frames.

On the same source, Release CTest passed 60/60. A Debug CTest run with
`ASAN_OPTIONS=detect_leaks=0 LSAN_OPTIONS=detect_leaks=0` passed 60/60.
Leak detection was disabled because this environment's LeakSanitizer failed
before test execution under its process tracing restrictions. Address and
undefined-behavior instrumentation remained enabled. No physical Blue was
available in the environment, so this result does not establish device
behavior or end-to-end Sapling signing.
