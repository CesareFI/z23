<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Ledger Blue replay result isolation

Date: 2026-09-28T11:06:05Z (2026-09-28T07:06:05-04:00).
Host CPU: AMD Ryzen 7 PRO 8840U w/ Radeon 780M Graphics.
Host compiler: Clang 22.1.6. ARM compiler: arm-none-eabi-gcc 16.2.0.

## Question

Can a replay API return a successful ZIP-243 digest while its result buffer
overwrites captured transaction hashes, another result, or a script input?

## Method

The three-pass replay and Blue payment-review finish functions now reject
fact and digest results that overlap each other, replay state, or a nonempty
script input. A nonempty script must be disjoint from replay state. The
post-review bound-input digest rejects output that overlaps replay state,
outpoint, or script, and a script that overlaps replay state. Checks run
before clearing outputs or invoking the hash callbacks. Finish alias errors
abort the review. Bound-digest alias errors leave the result and replay
unchanged, so the caller can safely abort the enclosing payment session.

The tests replay a valid transparent transaction and inject partial result
overlap, results inside captured replay state, and script input inside replay
state. They check rejection and state failure. A valid bound-input digest
still matches the reference after rejected aliases. Host builds run with
AddressSanitizer and UndefinedBehaviorSanitizer in Debug.

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

Serial Release passed 57/57 Blue cases in 12.62 seconds; sanitized Debug
passed 57/57 in 25.66 seconds. An earlier Release run timed out only on the
Cortex M0 note-commitment model under host load; that case passed alone in
3.50 seconds before the complete serial pass. LeakSanitizer was disabled
because this container blocks its ptrace setup; AddressSanitizer and
UndefinedBehaviorSanitizer remained active.

Wallet 0.3.13 linked with 44,032 bytes of `.text`, zero `.data`, and 5,120
bytes of `.bss`, including the 2,048-byte stack reservation. The largest
checked payment upload and previous-finish paths used 1,048 bytes plus the
required 512-byte margin. The `.text` SHA-256 is
`4c03fa4e1944c9182f4839e2b8ea26fc4befe6ad86f957ab649077e6aa664d08`;
the Intel HEX SHA-256 is
`622e08b08afa08529ee11d5e2371eed7576528e79c29244676419010daa8a595`.
At 2026-09-28T11:18:39Z (2026-09-28T07:18:39-04:00), a second source
checkout at commit `f62a9fd93fddea7a4648ff72f58dcc9c24464426` built
against a separately patched copy of the pinned Blue SDK. Both `.text` bytes
and the Intel HEX file matched exactly. Both builds used this host and
toolchain, so cross-toolchain reproducibility is untested. All 33 fast lint
gates passed without changing the complexity cap or baseline; the sealed
consensus core matched 554 files and 80 sections.

This checks API-owned memory separation on host and the pinned Blue SDK's
linked size and static stack paths. The Wallet remains uninstalled and has
not exercised this path on physical Blue firmware. Transaction inclusion,
UTXO status, and branch activation are still not device-proven.
