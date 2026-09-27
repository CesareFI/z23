<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# C23 development for Ledger Blue

Z23 builds Blue device apps from C23 source with Ledger's open-source
Blue SDK and open-source ARM tools. The host transport, tests, and installer
are C23. No Ledger Live service is part of the build or runtime path.

## Reproduce the toolchain

Use Ledger's `blue-secure-sdk` tag `blue-r21.1`, commit
`3c710b4c62ad847599a2deb0932a50dd1ae4bdff`, and apply the reviewed
[`blue-sdk-c23.patch`](device-blue/blue-sdk-c23.patch). The patch sets the
SDK's C dialect and ARM include path. The build used Clang 22.1.6,
`arm-none-eabi-gcc` 16.2.0, and ARM Newlib headers. Set `BOLOS_SDK` to the
patched SDK, `ARM_INCLUDE_DIR` to the Newlib include directory, and
`GCCPATH` and `CLANGPATH` to the compiler bin directories, each ending in
`/`. The Review app additionally requires its
[`blue-review-stack.patch`](device-blue-review/blue-review-stack.patch)
on a separate copy of the patched SDK. This reserves 2 KiB for the app
stack. The build rejects the base SDK's 1 KiB stack reserve.

```sh
cmake -S apps/zcl-ledger -B build/zcl-ledger-debug \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang
cmake --build build/zcl-ledger-debug
ctest --test-dir build/zcl-ledger-debug --output-on-failure
build/zcl-ledger-debug/test-blue-review-simulator \
  apps/zcl-ledger/tests/fixtures/zip243-transparent-vector3.hex

make -C apps/zcl-ledger/device-blue-review \
  BOLOS_SDK=/path/to/patched/blue-secure-sdk \
  ARM_INCLUDE_DIR=/path/to/arm-none-eabi/include \
  GCCPATH=/path/to/arm-toolchain/bin/ \
  CLANGPATH=/path/to/clang/bin/
```

The device Makefile requires `-std=c23 -Wall -Wextra -Werror -pedantic`.
The Blue linker rejects an SRAM overflow, and the Makefile rejects a
nonempty initialized `.data` section. It also compiles with
`-fstack-usage` and runs a C23 stack-budget check across the deepest
known APDU and screen call paths with a 512-byte reserve for intermediate
frames. This is a conservative build gate, not a complete firmware stack
proof. The code image is the ELF's `.text`
section, extracted with `arm-none-eabi-objcopy -O binary
--only-section=.text`. Record its byte count and SHA-256. The C23 installer
accepts only explicitly pinned hashes; source changes require a new app
version and pin. An exact image hash identifies the built bytes, not their
security.

## Separate evidence gates

Host unit tests exercise protocol bounds, transaction parsing, screen text,
address derivation, and cryptographic vectors. The C23 Review simulator
uses the same app controller source as the Blue image and prints each page
for the published 245-byte ZIP-243 transparent fixture. It exercises the
USB command sequence and page transitions through host hash callbacks.
It does not emulate BOLOS, the Blue USB transport, BAGL rendering, or touch.
An ARM build measures code,
initialized data, and SRAM use. Neither proves a Blue screen renders or
responds to touch. Test a new image on the dedicated Blue only after those
checks, then record install, open, page navigation, EXIT, USB replies, and
recovery observations for that exact image hash. Never promote a synthetic
transaction review to payment authorization.

The intended everyday app is one [ZCL Wallet](ROADMAP.md). The existing
Probe, Fixture, Review, and Sign Test apps are narrow development tools.
The Review app has no key permission; Sign Test can sign only its fixed
self-test message. The host must establish ZCL consensus branch, input
provenance, shielded output details, and token validity before asking a
future payment app to approve a transaction. A device approval must bind
the displayed recipient, amount, fee, account, and network to the exact
transaction bytes and the resulting signature.

Review 0.4.0 passed local tests but stopped answering USB and EXIT on the
dedicated Blue. It was deleted after a restart and its installer hash was
removed. Version 0.4.2 is an offline candidate; its host simulator and
stack gate improve pre-device evidence, but do not establish BOLOS touch
or USB responsiveness. It is not pinned for installation.
