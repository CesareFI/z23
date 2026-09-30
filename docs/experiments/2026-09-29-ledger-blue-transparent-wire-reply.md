<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->
# Ledger Blue transparent review wire commitment

## Question

Can the host verify that the complete unsigned transaction accepted by the
device review equals the exact host wire before previous-output binding and
signing?

## Result

The former finish reply contained only an output count. Wallet 0.3.44 returns
that count followed by the 32-byte SHA-256 commitment retained by its
three-pass replay. The host compares all 33 reply bytes against the frozen
review plan. A test changes one byte of the device's finish commitment while
preserving the success status; the host rejects the review, sends ABORT, and
the device test state has no verified review or ready fee. A successful
fixture compares the reply commitment with a separately calculated SHA-256
of its entire unsigned wire.

The device calculates the commitment from its received bytes during each
review pass and accepts the replay only if all three pass hashes match. The
new reply binds the host's subsequent previous-transaction upload to that
device-observed wire. The comparison does not attest that the USB peer is a
genuine Ledger Blue; the development secure channel has no pinned
manufacturer root.

## Reproduction

At 2026-09-29T05:46:26-04:00 (2026-09-29T09:46:26+00:00), on AMD Ryzen 7
PRO 8840U with Clang 22.1.6 and arm-none-eabi-gcc 16.2.0:

```sh
cmake --build /tmp/z23-blue-standalone-release -j4
ctest --test-dir /tmp/z23-blue-standalone-release -j1 --output-on-failure
ASAN_OPTIONS=detect_leaks=0 LSAN_OPTIONS=detect_leaks=0 \
  ctest --test-dir /tmp/z23-blue-standalone-debug -j1 --output-on-failure
make -C apps/zcl-ledger/device-blue-wallet \
  BOLOS_SDK=/tmp/z23-blue-sdk-repro-20260927 \
  ARM_INCLUDE_DIR=/tmp/z23-arm-toolchain/usr/arm-none-eabi/include \
  GCCPATH=/tmp/z23-arm-toolchain/usr/bin/ CLANGPATH=/usr/bin/ -j2
/tmp/z23-arm-toolchain/usr/bin/arm-none-eabi-objcopy -O binary -j .text \
  apps/zcl-ledger/device-blue-wallet/bin/app.elf \
  /tmp/z23-blue-wallet-0344.text
/tmp/z23-blue-standalone-release/zcl-blue-install \
  --image-check /tmp/z23-blue-wallet-0344.text
```

The pinned SDK is commit `3c710b4c62ad847599a2deb0932a50dd1ae4bdff`
with patch SHA-256
`4919fd81ba7a3a80880065aaf7898c2edd603b2586f6086a88570c73996019f6`.
The Wallet 0.3.44 ARM image uses 53,504 bytes of `.text`, zero `.data`, and
5,120 bytes of `.bss`. Its `.text` SHA-256 is
`d5288c8cbc2e1e5fc2f47d6d11d14796744edf3346effd5f7eb937d65e4461a8`;
its Intel HEX SHA-256 is
`dd0b0cf0f72c1d2797941f16f198a479a6a57a63d260770f5a9f54c09e051c86`.
The ARM stack checker reports a maximum modeled payment finish path of 1,112
bytes plus a required 512-byte margin in a 2,048-byte reservation. BOLOS
frames are excluded. The offline image check identifies a signing path and
reports installation blocked pending physical validation.

A fresh `git clone --no-hardlinks` of signed commit
`8fba3a05328a37c49239782010e79571a40080f5` was built with the separate
SDK copy at `/tmp/z23-blue-sdk-repro-independent-20260927`. Its extracted
`.text` and Intel HEX SHA-256 values both matched those above exactly.

The Release suite passed 62/62 in 28.99 seconds. The sanitized Debug suite
passed 62/62 in 33.47 seconds. Leak detection was disabled because this
environment's LeakSanitizer cannot run under its process tracing restrictions;
address and undefined-behavior instrumentation remained enabled. No physical
Blue USB interface was available. This work does not establish physical
open, touch, EXIT, disconnect, or signing behavior.
