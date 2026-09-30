<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Blue bound payment loop

Local time: 2026-09-28T05:00:00-04:00

UTC: 2026-09-28T09:00:00Z

The integrated host test now drives the actual Blue wallet `main.c` and
payment controller through the app's APDU dispatcher and synthetic touch
releases. A C23 test fixture supplies one previous transaction and an unsigned
transaction with two outputs: 1 ZCL to the device-derived account, 2 ZCL to
another address, and a 1 ZCL fee. The test uses the repository's BLAKE2b,
SHA-256, and RIPEMD-160 code; the fixture carries no private key. A separate
host preflight derives the expected ZIP-243 digest from the previous wire.

The test replays the complete transaction three times, checks both output
pages, requires a touch for each page, binds the previous wire, and compares
the device-side digest record against the host preflight. It visits fee,
totals, and final review pages. One route ends with NO SIGN and verifies that
a later signing APDU is refused. Another touches SIGN ZCL, sends a USB reset
through `io_event`, verifies that the APDU buffer and approval are erased,
and verifies that the signing APDU is then refused. The signer stub asserts
if called, so both routes also check that no signing takes place.

Clang 22.1.6 on an AMD Ryzen 7 PRO 8840U passed 54/54 Blue CTest cases in
Release and 54/54 in sanitized Debug. The integrated executable links only
libc at runtime. Debug used `ASAN_OPTIONS=detect_leaks=0` because
LeakSanitizer cannot start under this runner's ptrace configuration;
AddressSanitizer and UndefinedBehaviorSanitizer remained active. The
cyclomatic complexity gate passed at its unchanged cap of 15, and all 33
`lint-fast` gates passed.

This test models BOLOS dispatch and touch geometry in a host process. It does
not execute BOLOS, verify physical screen timing, establish chain provenance,
produce a real device signature, or test a shielded spend. The display still
warns `CHAIN UNCHECKED`; physical testing remains required before installing
this image on the Blue.

Reproduction from the Blue source checkout:

```sh
cmake -S apps/zcl-ledger -B /tmp/z23-blue-release -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/z23-blue-release --target test-blue-wallet-integrated-loop
ctest --test-dir /tmp/z23-blue-release -R '^blue-wallet-integrated-loop$' --output-on-failure
```
