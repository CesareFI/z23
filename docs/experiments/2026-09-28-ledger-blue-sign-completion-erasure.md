<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Ledger Blue final-signature cleanup

Date: 2026-09-28T11:56:57Z (2026-09-28T07:56:57-04:00).
Host CPU: AMD Ryzen 7 PRO 8840U w/ Radeon 780M Graphics.
Host compiler: Clang 22.1.6. ARM compiler: arm-none-eabi-gcc 16.2.0.

## Question

After the last transparent signature succeeds, does the Blue retain hash
contexts or formatted transaction facts in RAM while showing its completion
screen?

## Method

The wallet uses its full payment-abort cleanup after the final successful
signature, then restores only the transaction-free completion screen flag.
The APDU reply is outside the erased workspace. The host SDK test fills both
hash contexts and four payment-text buffers with nonzero bytes before the
last signing command. It checks exact signature reply bytes, erasure of those
buffers and the payment controller, and the “SIGNATURES READY” page. A
two-input test checks that the first signature preserves approval and the
second triggers complete cleanup.

Commands from the repository root:

```sh
cmake --build /tmp/z23-blue-standalone-release -j2
ctest --test-dir /tmp/z23-blue-standalone-release -j1 --output-on-failure
cmake --build /tmp/z23-blue-standalone-debug -j2
ASAN_OPTIONS=detect_leaks=0 ctest --test-dir /tmp/z23-blue-standalone-debug -j1 --output-on-failure
make -C apps/zcl-ledger/device-blue-wallet \
  BOLOS_SDK=/tmp/z23-blue-sdk-repro-20260927 \
  ARM_INCLUDE_DIR=/tmp/z23-arm-toolchain/usr/arm-none-eabi/include \
  GCCPATH=/tmp/z23-arm-toolchain/usr/bin/ CLANGPATH=/usr/bin/ -j2
```

## Result and limit

The focused host SDK UI test passed in Release and sanitized Debug. The
complete Release suite passed 57/57 Blue cases in 28.16 seconds; sanitized
Debug passed 57/57 in 71.93 seconds. LeakSanitizer was disabled because this
runner blocks its ptrace setup; AddressSanitizer and UndefinedBehaviorSanitizer
remained active. The sealed core matched 554 files and 80 sections. Wallet
0.3.15 linked with 44,032 bytes of `.text`, zero `.data`, and 5,120 bytes
of `.bss`, including its 2,048-byte stack reservation. The largest checked
payment upload path uses 1,064 bytes plus the required 512-byte margin.
The `.text` SHA-256 is
`95b97aeae1f58ec4f1864c1e3197c971a6f0fc1036f932d1d4bca6ead14ea5a0`;
the Intel HEX SHA-256 is
`877ad350f04827e437b11323b323bd41891429ebfbab213e5286f01d23f94e71`.
At 2026-09-28T12:01:50Z (2026-09-28T08:01:50-04:00), a second source
checkout at commit `35a2c26ba916fa8e5dd1fbca2afcd9b5f25eea4f` built
against a separately patched copy of the pinned Blue SDK. Both `.text` and
Intel HEX bytes matched the first build exactly. Both builds used the same
host and toolchain, so cross-toolchain reproducibility remains untested.
All 33 fast lint gates, the unchanged sealed core, and both Markdown gates
passed; inline paths had 0 new findings across 507 documents.

The cleanup preserves the returned signature bytes and completion page in
the host SDK test. It has not run on a physical Blue. Ledger OS and BOLOS
internals are outside this memory observation.
