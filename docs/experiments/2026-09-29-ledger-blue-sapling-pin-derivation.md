<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->
# Ledger Blue Sapling seed derivation after PIN loss

## Question

Can the isolated Blue ZIP32 seed adapter return a Sapling root when PIN
validation ends inside the BOLOS BIP32 derivation call?

## Experiment and correction

The host BOLOS shim filled a hardened BIP32 node, then revoked PIN
validation before returning. Before correction, the seed adapter accepted
the returned node and produced an extended Sapling key; the test failed at
its required rejection assertion. The adapter now checks PIN validation
again after BOLOS returns. The bridge treats that false result as a failed
derivation and erases both the extended key and its node workspace. The
regression checks the exact zeroed structures and that the BOLOS derivation
was called once during the revoked attempt.

This adapter is not linked into the Wallet image and grants no Sapling
signing APDU. It derives a Ledger-specific ZIP32 root, different from the
standard root of a wallet seed. The BOLOS key syscall may throw rather than
return; a future device caller must still clear key material in its outer
exception path.

## Reproduction and limits

Run at 2026-09-29T03:33:14-04:00 (2026-09-29T07:33:14+00:00) with Clang
22.1.6 on an AMD Ryzen 7 PRO 8840U:

```sh
cmake --build /tmp/z23-blue-standalone-release -j2 --target test-blue-zip32-master
ctest --test-dir /tmp/z23-blue-standalone-release -R '^blue-zip32-master$' --output-on-failure
/usr/bin/clang --target=armv7m-none-eabi -mcpu=cortex-m3 -mthumb \
  -std=c23 -Wall -Wextra -Werror -pedantic -fsyntax-only \
  -DIO_HID_EP_LENGTH=64 \
  -I/tmp/z23-blue-sdk-repro-20260927/include \
  -Iapps/zcl-ledger/include \
  -Iapps/zcl-ledger/device-blue-wallet/candidate \
  -Icore/modules/sapling/include \
  apps/zcl-ledger/device-blue-wallet/candidate/blue_zip32_seed_device.c
```

The focused host test passed, Release and sanitized Debug CTest each passed
60/60 cases, and the candidate compiled cleanly for the
Cortex-M3 target against the pinned Blue SDK headers. The ARM command checks
syntax and types only; it does not link or execute on BOLOS. No physical
Blue PIN-loss event or Sapling account derivation has been tested.
Debug used `ASAN_OPTIONS=detect_leaks=0 LSAN_OPTIONS=detect_leaks=0` because
the environment's LeakSanitizer fails before test execution under process
tracing; address and undefined-behavior checks remained active.
All 33 fast lint gates, the unchanged consensus-core seal, cyclomatic
complexity cap 15, and both Markdown gates passed.
