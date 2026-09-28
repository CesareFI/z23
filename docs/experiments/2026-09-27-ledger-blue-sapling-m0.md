<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Cortex-M0 Sapling signing fixture

## Intention

Check whether the isolated C23 Sapling SpendAuth signing route fits the
Ledger Blue class of ARM CPU and a 2 KiB stack reservation. Keep the test
independent of the installed Wallet app until transaction review and final
approval are connected to signing.

## Reproduction

Run on the Z23 checkout with ARM GNU Toolchain 16.2.0, QEMU 11.0.1, and
Clang Debug with AddressSanitizer and UndefinedBehaviorSanitizer:

```sh
cmake -S apps/zcl-ledger -B build/zcl-ledger-debug \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang \
  -DBLUE_ARM_GCC=/absolute/path/arm-none-eabi-gcc
cmake --build build/zcl-ledger-debug -j4
ctest --test-dir build/zcl-ledger-debug --output-on-failure
```

The ARM cases run the same public fixture on QEMU `microbit` Cortex-M0 and
`mps2-an385` Cortex-M3 models. The linker bounds simulated RAM at 6 KiB and
reserves 2 KiB for stack. The test fails when its stack watermark crosses
the 1536-byte ceiling, leaving 512 bytes of reserve.

## Observation

On 2026-09-27T23:47:31-04:00 (2026-09-28T03:47:31Z), an AMD Ryzen 7 PRO
8840U host completed all 53 CTests. The Cortex-M0 fixture reported `M0 PASS`
with a 1512-byte peak stack watermark; Cortex-M3 reported `M3 PASS` with a
1436-byte peak. Both matched the public SpendAuth vector, the ZIP32 child
and seed bridge, the mapped spend signature, and the consensus spend
signature. The previous Cortex-M0 peak was 1608 bytes under the same test
fixture; reusing field and signer scratch buffers reduced it by 96 bytes.
The host tests also cover in-place challenge, response, and signature
outputs. The repository's cyclomatic-complexity check passes at the
15-branch cap.

## Limit

QEMU executes isolated arithmetic and signing with public fixture secrets.
It does not execute BOLOS, the Ledger SDK, seed access, Wallet transaction
review, or final user approval. These measurements do not establish that a
physical Blue can sign an approved Sapling payment. The next integration
test must bind the verified transaction digest and displayed review facts
to a device-approved signer request before any hardware signing trial.
