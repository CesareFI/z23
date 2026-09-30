<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->
# Ledger Blue Sapling device entropy adapter

## Question

Can an isolated C23 adapter obtain SpendAuth nonce entropy from the Blue
CSPRNG while failing closed on locked, interrupted, empty, or failed calls?

## Experiment

The adapter requests 80 bytes through BOLOS `cx_rng` only after PIN
validation. It checks the returned pointer, checks PIN validation again,
rejects an all-zero buffer, and clears output on every returned failure.
The caller owns the buffer and must erase it after use. The adapter does
not take host-supplied entropy and is not linked into the Wallet image.

A host syscall shim tests a locked device, successful 80-byte output,
all-zero output, a null RNG result after a partial write, a mismatched
returned pointer, PIN loss inside the RNG call, and a null output pointer.
The candidate also compiles as ISO C23 for Cortex-M3 against the pinned
Blue SDK headers. Cortex-M0 and Cortex-M3 emulator cases run the same
adapter with fake BOLOS calls and check success, locked rejection, PIN-loss
rejection, and output erasure. The mapped-key SpendAuth fixture obtains its
nonce entropy through this adapter before comparing its exact public
signature. That fixture maps a synthetic BIP32 node into a ZIP32 child,
then checks `rk` before signing the fixed digest.

## Results and limits

At 2026-09-29T03:41:07-04:00 (2026-09-29T07:41:07+00:00), the host was an
AMD Ryzen 7 PRO 8840U with Clang 22.1.6. The ARM toolchain was
arm-none-eabi-gcc 16.2.0 and QEMU 11.0.1. Release and sanitized Debug CTest
each passed 61/61 tests. Debug used
`ASAN_OPTIONS=detect_leaks=0 LSAN_OPTIONS=detect_leaks=0` because this
environment's LeakSanitizer fails under process tracing before test
execution; address and undefined-behavior instrumentation remained active.

Both Cortex-M3 and Cortex-M0 emulator runs passed. Each `ENTROPY` case used
at most 512 bytes of the emulator's 2,048-byte stack by its watermark. The
mapped SpendAuth case used 1,396 bytes on M3 and 1,488 bytes on M0. The
complete fixture runs peaked at 1,404 bytes on M3 and 1,512 bytes on M0;
these peaks include other Sapling cases. The adapter has not been linked
with BOLOS or run on a physical Blue. Its fake RNG output is a deterministic
test vector and does not establish physical RNG quality. A BOLOS exception
can bypass ordinary returns, so a future device caller must erase the
caller-owned entropy buffer in its outer exception cleanup. No signing APDU
or payment approval uses this adapter.
