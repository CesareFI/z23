<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->
# Ledger Blue device-derived SpendAuth candidate

## Question

Can one bounded C23 device candidate derive a Ledger-specific hardened
Sapling account key, obtain nonce entropy from BOLOS, and produce the public
mapped-key SpendAuth signature while clearing secrets and rejecting PIN loss?

## Experiment

`blue_sapling_device_spend_sign` composes the isolated BOLOS BIP32 seed
bridge, ZIP32 child derivation, BOLOS RNG adapter, and checked-rk SpendAuth
signer. Its caller supplies a device-verified ZIP-243 digest, transaction rk,
Sapling ar, and an account number below the hardened limit. The candidate
rejects overlapping input, signature, and workspace storage before writes;
it erases the workspace on every returned path and erases a failed signature.
It checks PIN validation after derivation, RNG, and signing. BOLOS exceptions
require an outer handler to erase the caller-owned signature and workspace.

The Cortex-M0 and Cortex-M3 emulator fixtures execute this composition with
deterministic fake BOLOS calls. They compare the exact public mapped-key
signature and reject a changed rk and locked device. A host fault test forces
PIN loss after the signer writes a signature, rejects an out-of-range account
and overlapping input, and checks failure erasure. The implementation now
requires `ZCL_BLUE_SYNTHETIC_SEED_FIXTURE`; an ordinary pinned-SDK build
intentionally fails. The Wallet's BOLOS stack-canary build flag also rejects
the fixture even if that test flag is set.

## Results and limits

At 2026-09-29T03:56:46-04:00 (2026-09-29T07:56:46Z), the host was an
AMD Ryzen 7 PRO 8840U using Clang 22.1.6, arm-none-eabi-gcc 16.2.0, and
QEMU 11.0.1. Release and sanitized Debug CTest each passed 62/62 tests.
Address and undefined-behavior sanitizers were active in Debug;
LeakSanitizer was disabled because this environment blocks its startup under
process tracing. Cortex-M3 and Cortex-M0 emulator fixtures both passed.
The mapped-signature case used 1,452 bytes of the 2,048-byte reserved M3
stack and 1,536 bytes of the M0 stack, leaving 596 and 512 bytes respectively.
The M0 value meets the fixture's minimum free-stack requirement exactly.
The emulator image has 2,716 bytes of BSS on M3 and 2,728 bytes on M0;
these are fixture figures, not Wallet image figures.

The scalar multiplier and RedJubjub signer have not passed target side-channel
validation for secret device material. A prior candidate could compile with
the Blue SDK despite this documented limit; the fixture-only build guard now
prevents that accidental device route. The candidate is not linked into Wallet,
has no signing APDU or physical
Blue result, and does not verify the supplied transaction digest, rk, ar,
screen facts, or user approval. Emulator BOLOS calls are deterministic fakes.
Production signing requires device replay of the transaction, exact
displayed-versus-signed binding, approval, interruption handling, and
physical-device memory and gesture validation.
