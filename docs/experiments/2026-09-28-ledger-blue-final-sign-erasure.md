<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Ledger Blue final signature state erasure

Date: 2026-09-28T10:40:08Z (2026-09-28T06:40:08-04:00).
Host CPU: AMD Ryzen 7 PRO 8840U w/ Radeon 780M Graphics.
Host compiler: Clang 22.1.6. ARM compiler: arm-none-eabi-gcc 16.2.0.

## Question

Can the Blue show that all transparent signatures were prepared without
retaining the completed transaction review, bound digests, and fee in RAM?

## Method

The wallet keeps an explicit completion screen flag. After the final
touchscreen-approved signing command succeeds, its timer and interruption
checks run, then the entire payment controller is erased before the APDU
reply is sent. The completed screen uses only the flag. A new review, EXIT,
USB reset, or rejected command clears the flag. The SDK-shim UI test checks
the complete controller byte-for-byte against an erased controller with only
its failure marker set, checks the completed screen, and refuses a second
signing request. A separate two-input case proves the first signature leaves
the second digest and approval live; the final signature erases the review.

Build and test commands from the repository root:

```sh
make -C apps/zcl-ledger/device-blue-wallet \
  BOLOS_SDK=/tmp/z23-blue-sdk-repro-20260927 \
  ARM_INCLUDE_DIR=/tmp/z23-arm-toolchain/usr/arm-none-eabi/include \
  GCCPATH=/tmp/z23-arm-toolchain/usr/bin/ CLANGPATH=/usr/bin/ -j2
ctest --test-dir /tmp/z23-blue-standalone-release -j1 --output-on-failure
ASAN_OPTIONS=detect_leaks=0 ctest --test-dir /tmp/z23-blue-standalone-debug -j1 --output-on-failure
```

## Result and limit

Version 0.3.12 linked with 43,264 bytes of `.text`, zero `.data`, and 5,120
bytes of `.bss`, which includes the 2,048-byte reserved stack. The checked
largest payment upload path used 1,048 bytes plus a separate 512-byte margin.
The `.text` SHA-256 is
`acf4614d8355512d2fdb1f1805d96163a6851537c726120d4cadd13ef08d5c08`;
the Intel HEX SHA-256 is
`984634ff4d1b47ac6d7d5b013c93bbec7c75a1be93c73ffc5d4fdfc739afb658`.
At 2026-09-28T10:51:20Z (2026-09-28T06:51:20-04:00), a second source
checkout at commit `4e23d1a3c8a44d89194cde74c79b3695f40c6753` built
against a separately patched checkout of the pinned SDK. Both `.text` bytes
and the Intel HEX file matched exactly. Both builds used this host and
toolchain; they do not test cross-toolchain reproducibility.

A serial Release run passed 57/57 Blue cases in 11.60 seconds; sanitized
Debug passed 57/57 in 107.69 seconds while another full suite was active.
The concurrent Release run timed out only on the Cortex M0 Sapling case; that
case passed alone in 1.34 seconds, then the full serial Release run passed.
LeakSanitizer was disabled because the container blocks its ptrace setup;
AddressSanitizer and UndefinedBehaviorSanitizer remained active. The stronger
whole-controller erasure assertion passed separately in Release and Debug
after the full-suite builds.
The final source passed all 33 fast lint gates without changing the complexity
cap or baseline. The sealed consensus core matched 554 files and 80 sections;
Markdown targets and inline paths passed across 504 documents.

The SDK shim tests UI and APDU control flow with fake device calls. The image
is uninstalled; its behavior after a real Blue touch, USB disconnect, and
power interruption is not yet measured. The installer does not admit this
new image pending policy review and physical validation.
