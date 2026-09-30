<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Ledger Blue payment APDU fuzzing

Local time: 2026-09-27T11:15:13-04:00

UTC: 2026-09-27T15:15:13Z

The C23 `fuzz-blue-payment-apdu` target splits each input into a sequence of
USB command frames and sends them through the same payment APDU parser used
by the Blue wallet. Some inputs begin with a valid, known-mainnet review
session; others replay a complete one-input, one-output transparent wire to
the first pending output page before the mutated commands. The harness
checks reply bounds,
that USB commands never set either touchscreen authorization latch, and that
all rejected commands erase active review and verified fee state. It uses
Z23's bundled C23 SHA-256 and BLAKE2b code; no OpenSSL function is linked
into the fuzzer.

At 2026-09-27T11:15:13-04:00 (2026-09-27T15:15:13Z), Clang 22.1.6 Debug
with libFuzzer, AddressSanitizer, and UndefinedBehaviorSanitizer completed
100,000 inputs with no finding. The final report showed 193 covered
instrumentation edges, 470 features, 39 corpus entries, and 52 MB peak RSS
on an AMD Ryzen 7 PRO 8840U. Reproduce locally with:

```sh
cmake -S apps/zcl-ledger -B /tmp/z23-blue-apdu-fuzz \
  -DCMAKE_C_COMPILER=clang -DCMAKE_BUILD_TYPE=Debug -DZCL_LEDGER_FUZZ=ON
cmake --build /tmp/z23-blue-apdu-fuzz --target fuzz-blue-payment-apdu
/tmp/z23-blue-apdu-fuzz/fuzz-blue-payment-apdu -runs=100000 -max_len=512
```

The 100,000-input result above predates the pending-output seed. The added
seed reached a pending output page in a focused one-input run; the extended
campaign reached 28,791 inputs without a reported finding before it was
stopped to free the host for the repository landing proof. It did not reach
its 100,000-input target. The C23 complexity ratchet passed 60,820 functions
at cap 15 after the seed was added.

On 2026-09-28T02:35:55-04:00 (2026-09-28T06:35:55Z), the current
pending-output harness completed 100,000 inputs in 30 seconds with no
finding. Clang 22.1.6, libFuzzer, AddressSanitizer, and
UndefinedBehaviorSanitizer ran on an AMD Ryzen 7 PRO 8840U. The final report
showed 9,287 covered instrumentation edges, 9,664 features, 33 corpus entries,
and 60 MB peak RSS. The runner required `ASAN_OPTIONS=detect_leaks=0` because
LeakSanitizer could not start under ptrace; this run does not establish leak
freedom. The exact command was:

```sh
ASAN_OPTIONS=detect_leaks=0 \
  /tmp/z23-blue-apdu-fuzz-20260928/fuzz-blue-payment-apdu \
  -runs=100000 -max_len=512 -seed=20260928 -print_final_stats=1
```

The campaign exercises malformed frames and partial review state. It does
not prove that all valid spending and previous transaction streams, physical
touch events, USB timing, BOLOS cryptography, or Sapling signing are safe.
