<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Ledger Blue transaction-upload alias isolation

Date: 2026-09-28T11:38:20Z (2026-09-28T07:38:20-04:00).
Host CPU: AMD Ryzen 7 PRO 8840U w/ Radeon 780M Graphics.
Host compiler: Clang 22.1.6. ARM compiler: arm-none-eabi-gcc 16.2.0.

## Question

Can transaction bytes be read from storage that the Blue's replay or review
parser mutates during the same upload?

## Method

Reject any nonempty feed buffer overlapping the replay state. The wallet
review also checks its enclosing state, including the displayed output. Each
check runs before parsing, observer callbacks, or hash updates. An overlap
aborts and erases the session.

Host tests provide aliased bytes that match the next valid transaction header
byte, once from the review's output and twice from replay state. The second
replay case spans two adjacent captured-hash fields. Both APIs must abort
before accepting those bytes; a fresh session must still accept normal input.

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

The two focused tests, `blue-payment-review` and `zcl-zip243`, passed in
Release and sanitized Debug builds. The complete Release suite passed 57/57
cases in 78.10 seconds, and sanitized Debug passed 57/57 in 92.81 seconds,
under shared-host load. LeakSanitizer was disabled because this runner blocks
its ptrace setup; AddressSanitizer and UndefinedBehaviorSanitizer remained
active. Wallet 0.3.14 linked with
44,032 bytes of `.text`, zero `.data`, and 5,120 bytes of `.bss`, including
the 2,048-byte stack reservation. The largest checked payment-upload path
uses 1,064 bytes plus the required 512-byte margin. The `.text` SHA-256 is
`9d71b563acbb12eba0a434627f7584a98cc81010fac12b4f0fad2f7fb82d37d7`;
the Intel HEX SHA-256 is
`14ce37c3d481a14d18dc4b0ca72b3506aa8dfc612fcfc7867fccdc3c968ef5d5`.
At 2026-09-28T11:51:31Z (2026-09-28T07:51:31-04:00), a second source
checkout at commit `b6da2a1fa9bb11149c8bd1b84c2fe4d10e75032f` built
against a separately patched copy of the pinned Blue SDK. Its `.text` and
Intel HEX bytes matched the first build exactly. Both builds used the same
host and toolchain, so cross-toolchain reproducibility remains untested. All
33 fast lint gates, the unchanged sealed core (554 files and 80 sections),
and Markdown links across 506 documents passed.

This tests caller-owned memory separation, fail-closed recovery, and the
pinned SDK image budget. It does not establish physical Blue behavior or
independent provenance for the host-provided previous transaction.
