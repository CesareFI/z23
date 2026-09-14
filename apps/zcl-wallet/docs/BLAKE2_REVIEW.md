# BLAKE2b public-data provider review

The exact two-line initialization repair described below passes the unchanged
strict checks and now backs an internal bounded public-data hash helper. The
original finding and evidence remain preserved. This qualification permits
only unkeyed sequential BLAKE2b-256, zero salt, exactly 16 personalization bytes
and at most 4096 input bytes. It is not a general keyed/tree-hashing, transaction
signature-hash, signing, custody or consensus qualification. The separate TLS
candidate remains **BLOCKED — REQUIRES FURTHER SECURITY REVIEW**.

## Original candidate and finding — 2026-09-13

Candidate: official BLAKE2/BLAKE2 commit
`ed1974ea83433eba7b2d95c5dcd9ac33cb847913`, obtained via public HTTPS on2026-09-13.
The official [BLAKE2 site](https://www.blake2.net/) links that source repository.
The downloaded `ref/blake2b-ref.c`, `ref/blake2.h`, `ref/blake2-impl.h`, `COPYING`
and `testvectors/blake2-kat.h` are unchanged. Hashes identify reviewed bytes;
no publisher-signature claim is made. The following describes that original
review, before the separate repair qualification on 2026-09-14.

Pre-integration Clang20 static analysis reported:

```text
blake2-impl.h:59:32: error: The left operand of '<<' is a garbage value
[core.UndefinedBinaryOperatorResult]
```

One bounded diagnostic follow-up with text output traced the path from the
`blake2` alias through unkeyed `blake2b`, `blake2b_init`, `blake2b_init_param`
and `load64` while reading the parameter block. The source assigns parameter
fields and clears reserved/salt/personal storage, so the diagnostic alone does
not establish a runtime uninitialized read. Whether this is a defect or analyzer
model limitation remains unresolved. No suppression, provider patch, changed
threshold or false-positive waiver was applied.

The review script stopped on this first analyzer failure. GCC analysis and
upstream ASan/UBSan/LSan self-test were not reached; no runtime crash, sanitizer
result or cryptographic failure is claimed. Host libsodium1.0.18 is available for
an eventual independent personalized-hash oracle, but no such oracle ran yet.

Evidence is preserved under
`.cache/android-wallet/48h-20260912T235829Z/blake2-review/`: exact source/license/
known-answer vectors, `SHA256SUMS`, original analyzer log, text path diagnostic,
and any generated analyzer artifacts. The parent area retains the public
upstream commit response, `check-blake2-provider.sh`, result1 and
`blake2-review-evidence.tar` with its separate SHA256 manifest. The archive
preserves the initial state without overwriting earlier wallet/TLS evidence.
Archive SHA256:
`0026d784348a7b998c57876839c37c03b3715a1e406b26efdd9980f262a5fad1`.

Primary source SHA256:
`e2bf9872a8f0a51711b765936d420a7f8c34797db4d938948c24f2ddbc1dc588`.
Header SHA256:
`389bc87a83cdd9e25569a294d01a3347970d117237a66eee9df8edd6058736a4`.
Byte-helper SHA256:
`bc0ead7f3259a415325fa40ddebb1876f903d5062d888fc5994e8b2d9e616ec4`.

## Exact repair and bounded integration — 2026-09-14

`vendor/android-blake2/blake2b-ref.c` adds only
`memset(P, 0, sizeof(P));` immediately after both `blake2b_param P[1]`
declarations, with a comment explaining byte initialization before `load64`.
This explicitly initializes the complete 64-byte parameter representation.
All original field assignments, compression rounds and serialization remain.
The headers and complete upstream license are unchanged. The parameter layout
has compile-time size checks; the wrapper additionally checks personalization's
offset of 48. No analyzer finding is suppressed or marked a false positive.

The original source reproduced the same Clang finding in a controlled baseline.
The repaired source passes Clang20 and GCC analysis with warnings as errors.
Patched source SHA256:
`60cf3112177e88279f984e452b80ec5050e54223d940afad9a39540c7a698fc5`.
`vendor/android-blake2/SHA256SUMS` is checked by the normal safety script.

`native/src/blake2_hash.c` checks all spans before provider access, initializes
the complete parameter/state objects, checks each init/update/final result and
publishes exactly 32 bytes only after success. State, parameters and temporary
digest clear on every provider exit. No allocation, retained handle, JNI entry
or secret-key input is introduced. Compression temporaries are public and do
not receive a secret-erasure claim. Original branch/context parity and
authenticated immutable review/key ownership remain signing prerequisites.

## Evidence and reproduction

The upstream ASan/UBSan/LSan self-test passes 32,768 keyed one-shot/streaming
checks. Separate fixtures pass 256 upstream unkeyed64-byte known answers and
560 personalized32-byte whole/chunk comparisons against host libsodium1.0.18.
The latter uses four personalization profiles, fourteen lengths from 0 to 4096
including block boundaries, and ten chunk sizes. The pre-integration provider
fuzzer completes 4,085,206 executions in 121 seconds without a finding.

The durable `native/tests/blake2_vectors.h` contains 56 independent public
libsodium vectors. `seed_blake2_vectors.c` generates them without including or
linking the reference provider. Reproduction matches every byte. Normal
`wallet_blake2` tests every vector at all 65 output capacities 0..64, input and
personalization immutability, NULL arguments, SIZE_MAX and oversize refusals.
`wallet_blake2_failures` dirties provider scratch at each failure ordinal,
asserts that subsequent stages stop, observes all three wipes while their
objects are alive and checks unchanged output. Six isolated mutants omit one
wipe, omit personalization, ignore init failure or publish a failed digest;
each fails its intended assertion. Wrapper fuzzing with the live libsodium
oracle completes 6,676,084 executions in 121 seconds without a finding.

From `apps/zcl-wallet`, the normal gates and optional host oracle are:

```sh
bash tools/check-c-safety.sh
cmake -S native -B native/build/blake2-oracle -DCMAKE_C_COMPILER=clang-20 \
  -DZCL_SANITIZE=ON -DZCL_ORACLE=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build native/build/blake2-oracle --target blake2_tests seed_blake2_vectors -j4
ctest --test-dir native/build/blake2-oracle -R '^wallet_blake2$' --output-on-failure
native/build/blake2-oracle/seed_blake2_vectors > native/build/blake2-oracle/vectors.h
cmp native/tests/blake2_vectors.h native/build/blake2-oracle/vectors.h
```

The optional oracle profile needs host OpenSSL 3 and libsodium development
libraries; neither is an Android runtime dependency. A separate Clang build
with `ZCL_SANITIZE=ON`, `ZCL_FUZZ=ON` and `ZCL_ORACLE=ON` builds `fuzz_blake2`.
Use a private corpus, `-max_len=4114 -max_total_time=120 -timeout=5
-rss_limit_mb=512` and a private artifact directory. Default fuzz builds still
check bounds/canaries; the optional oracle adds independent digest comparison.

All 65 native tests pass under ASan/UBSan/LSan in 45.68 seconds. Both analyzers
pass the wrapper, provider and every fixture mode. Production/test complexity
caps remain 10/15 (458/863 functions). Host optimized stack reports show 376 bytes
for the wrapper and at most 216 bytes for its directly used provider functions;
these are individual compiler frame observations, not a whole-call-chain bound.

NDK 27 builds ARM64 and x86-64 archives and standalone test executables with
16 KiB ELF load alignment, RELRO, immediate binding and a non-executable stack.
The x86-64 standalone executable links the actual Android release core/provider
archives and passes all 56 vectors, 65 capacities and span refusals on API 30,
API 35 and API 36. Its exact hash is
`9fc5725c7174b835a04ef76c9d3c56057b77d86ca33701805de9f13e93d7af8d`.
ARM64 is compiled, not device-executed. A separate earlier provider-only
executable also passes the 256 unkeyed KATs and 560 chunk comparisons on each
emulator. All data is public; neither executable accesses a wallet or Keystore.

JVM tests, Android build/lint, APK alignment/fixture isolation and architecture
checks pass. Debug, test and unsigned release APKs are byte-identical to their
preceding artifacts: the unused helper/provider is not pulled into JNI. These
device observations qualify the standalone C helper, not a new APK feature.

Evidence is preserved in `.cache/android-wallet/blake2-init-20260914/` and
`.cache/android-wallet/blake2-wrapper-20260914/`, including the exact original
and patched sources, vectors, analysis, sanitizer/fuzz logs, mutation failures,
Android archive/executable identities and bounded device execution logs.
The full hazard review is in [C_SAFETY_REVIEW.md](C_SAFETY_REVIEW.md).
