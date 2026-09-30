<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Ledger Blue signing-command fuzzing

Local time: 2026-09-28T02:39:43-04:00

UTC: 2026-09-28T06:39:43Z

The C23 `fuzz-blue-payment-sign` target exercises the exact `A5 29`
signing-command parser with malformed frames, insufficient reply capacity,
overlapping request and reply buffers, signer refusal, invalid DER, an invalid
compressed-public-key prefix, and both unapproved and touchscreen-approved
state. Its built-in empty seed completes a valid one-input signing command;
the positive oracle requires that path to succeed. A second identical command
must fail without invoking the signer again. Rejected commands must clear the
approval, fee, review state, and reply bytes.

The signer callback returns a deterministic DER fixture with `r = s = 1`.
It is a parser and authorization test; the fixture is not a cryptographic
signature of the reviewed digest. The separate signing test checks a real
secp256k1 signature and host verification.

Clang 22.1.6, libFuzzer, AddressSanitizer, and UndefinedBehaviorSanitizer
completed 100,000 inputs with no finding on an AMD Ryzen 7 PRO 8840U. The
report showed 413 covered instrumentation edges, 470 features, 28 corpus
entries, and 50 MB peak RSS. LeakSanitizer was disabled because it cannot
start under this runner's ptrace environment. Reproduce with:

```sh
cmake -S apps/zcl-ledger -B /tmp/z23-blue-apdu-fuzz-20260928 \
  -DCMAKE_C_COMPILER=clang -DCMAKE_BUILD_TYPE=Debug -DZCL_LEDGER_FUZZ=ON
cmake --build /tmp/z23-blue-apdu-fuzz-20260928 \
  --target fuzz-blue-payment-sign -j2
ASAN_OPTIONS=detect_leaks=0 \
  /tmp/z23-blue-apdu-fuzz-20260928/fuzz-blue-payment-sign \
  -runs=100000 -max_len=512 -seed=20260928 -print_final_stats=1
```

This result does not test BOLOS key derivation, physical USB and touch,
power loss, a live UTXO, or the exact app image installed on a Blue.
