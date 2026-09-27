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

The device Makefiles require `-std=c23 -Wall -Wextra -Werror -pedantic`.
The Blue linker rejects an SRAM overflow, and the Makefile rejects a
nonempty initialized `.data` section. It also compiles with
`-fstack-usage` and runs a C23 stack-budget check across the deepest
known APDU and screen call paths with a 512-byte reserve for intermediate
frames. The Wallet receive candidate uses the same checker for its
derivation, address formatting, APDU, and event paths. This is a conservative
build gate, not a complete firmware stack proof. The code image is the ELF's `.text`
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
font; `--dark` previews a high-contrast dark palette. The uninstalled
Review 0.4.3 ARM candidate exposes the matching controls. The renderer
shares layout constants with the ARM app and decodes an Apache-2.0 Ledger
Open Sans BAGL font table. The
table is the closest available open-source match; its identity with the
font inside the dedicated Blue's BOLOS 2.1.1 is unverified. Rounded button
edges and the antialias palette are approximations. These PNGs test layout
and clipping, not physical pixel equality. It does not emulate BOLOS, the
Blue USB transport, or touch.
The host CLI exposes the same controller exercise through `--simulate-app`;
its JSON keeps simulated and physical Blue evidence in separate fields.

## Reusable Blue screen preview

[`blue_bagl_canvas.h`](include/blue_bagl_canvas.h) provides a C23 host
canvas for the Blue's 320 × 480 screen. It draws clipped rectangles,
rounded rectangles, and ASCII text with the two reviewed BAGL font tables;
it writes RGB PNGs through libpng. The nominal 14-pixel font table has a
16-pixel bitmap height; the larger table has a 22-pixel bitmap height.
`blue_bagl_text` rejects unsupported bytes, overlong labels, and text boxes
outside the screen instead of silently truncating them. Future app preview
renderers can link `blue_bagl_canvas` and keep their app-specific layout in a
separate source file. Review now uses this API; its 68 fixture PNGs across
regular, large-text, dark, and large-text dark modes are byte-identical to
the previous renderer.
`blue_bagl_wrap_ascii` uses actual glyph advances and explicit input length,
output capacity, line width, and line count. It wraps at spaces where
possible, breaks long words when needed, and rejects unsupported bytes or
content that cannot fit. Its output is only valid when it returns true.

The canvas is host-only and does not access the Blue framebuffer. Its PNGs
show intended layout using SDK font data; physical font pixels, touch
regions, contrast, and responsiveness require a dedicated-device check for
the exact ARM image hash. Future memo displays must classify UTF-8 and
opaque bytes before using this ASCII-only canvas. A payment app must show
and bind every material transaction field before any key operation.

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

## Transparent input provenance gate

[`zcl_tx_prevout.h`](include/zcl_tx_prevout.h) is a C23 host preflight for
unsigned, all-transparent v4 transactions with up to 16 P2PKH inputs and
standard P2PKH/P2SH outputs. The caller supplies each complete previous v4
transaction in input order. Preflight parses those transactions, computes
each SHA-256d txid, compares the exact outpoint bytes, selects the indexed
P2PKH output, rejects duplicate outpoints, and calculates input total,
output total, and fee. Its hash-bound ZIP-243 digest takes script and amount
from that selected output. The tests compare the double hash of ZIP 243's
published vector 3 with its published txid, check a synthetic fee and input
digest, and reject mismatches and truncated encodings.

A previous transaction matching an outpoint does not prove the output is
unspent, mature, on the accepted ZCL chain, or controlled by the selected
Ledger account. The caller-provided branch ID also needs consensus-height
validation. The Review app cannot verify previous transactions and cannot
sign. This preflight must not set `signing_ready` or supply approval text to
a signer. The next device design must stream authenticated prevout evidence,
derive change scripts from its own key path, independently display every
output and fee, and bind final approval to its own digest computation.

For parser fuzzing with the published transparent fixture as the initial
corpus, build `fuzz-zcl-tx` with Clang Debug and `ZCL_LEDGER_FUZZ=ON`, then
run it locally with a fixed seed. The fuzzer checks that complete-transaction
parsing, input/output visitors, script classification, and ZIP-243 hashing
agree on accepted v4 bytes. It does not fuzz BOLOS or prove consensus parity.

Review 0.4.0 passed local tests but stopped answering USB and EXIT on the
dedicated Blue. It was deleted after a restart and its installer hash was
removed. Version 0.4.3 adds font-size and palette controls to the offline
candidate. Host tests check navigation, wrapping, and screenshots; they do
not establish BOLOS touch or USB responsiveness. It is not pinned for
installation.

The ST31G480 silicon supports up to 28 MHz, 12 KiB user RAM, and 480 KiB
secure user flash. Those are silicon limits, not measured app throughput or
free install capacity. The patched Blue linker grants this app 6 KiB SRAM
and a 400 KiB code address range. Review 0.4.3 uses 6,000 SRAM bytes
(including a 2,048-byte stack reserve) and 32,256 code bytes, leaving 144
bytes in the SRAM region. Its APDU review limit is 2,304 bytes. No on-device
latency measurement has been made.
