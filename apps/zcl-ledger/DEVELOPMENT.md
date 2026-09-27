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
build/zcl-ledger-debug/zcl-tx-review --json --simulate-app \
  transaction.bin
build/zcl-ledger-debug/zcl-blue-screen-sim \
  transaction.bin /tmp/zcl-blue-preview
build/zcl-ledger-debug/zcl-blue-screen-sim \
  transaction.bin /tmp/zcl-blue-accessible --large-text --dark

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
The C23 renderer writes one 320 × 480 PNG for the waiting screen, each
transaction page, and the wraparound page. `--large-text` previews each
nonempty detail on a separate screen in the SDK's 22-pixel full-alphabet
font; `--dark` previews a high-contrast dark palette. These two switches
are host previews and are not selectable on the Blue yet. The renderer
shares layout constants with
the ARM app and decodes an Apache-2.0 Ledger Open Sans BAGL font table. The
table is the closest available open-source match; its identity with the
font inside the dedicated Blue's BOLOS 2.1.1 is unverified. Rounded button
edges and the antialias palette are approximations. These PNGs test layout
and clipping, not physical pixel equality. It does not emulate BOLOS, the
Blue USB transport, or touch.
The host CLI exposes the same controller exercise through `--simulate-app`;
its JSON keeps simulated and physical Blue evidence in separate fields.
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

The ST31G480 silicon supports up to 28 MHz, 12 KiB user RAM, and 480 KiB
secure user flash. Those are silicon limits, not measured app throughput or
free install capacity. The patched Blue linker grants this app 6 KiB SRAM
and a 400 KiB code address range. Review 0.4.2 uses 6,024 SRAM bytes
(including a 2,048-byte stack reserve) and 29,696 code bytes, leaving 120
bytes in the SRAM region. Its APDU review limit is 2,432 bytes. No on-device
latency measurement has been made.
