# TLS candidate: BLOCKED — REQUIRES FURTHER SECURITY REVIEW

The owner authorized resuming host security review on 2026-09-30. Android/JNI
TLS remains disabled; permission to review is not acceptance for wallet use.
The original parked investigation and validated wallet candidate remain
preserved separately. No sanitizer or analyzer gate has been weakened.

## Resumed empty-name review

The original saved input reproduces UBSan in the OID comparison. The previously
parked OID-only patch then reproduces UBSan in the following value comparison.
The candidate now uses one zero-length-safe byte comparison for both calls.
Nonempty comparison, lengths, tags, text encodings and list structure retain
their prior behavior. Both existing comparator complexities remain 10; the
new helper is 2. No allocation, cryptography or consensus logic changes.

The saved flight is still rejected. Host sanitizer regressions also cover
empty/null-backed buffers, unequal bytes/lengths/tags, permitted text-case
equivalence, merged sets and unequal lists. Mutations restoring either unsafe
call, deleting the helper guard, or ignoring nonempty differences are caught.
Existing authenticated TLS and failure/cleanup fixtures pass. Broader provider
analysis remains a separate unresolved gate; these results do not enable TLS.
Provider original/current hashes and the reversible local patch are recorded
in `vendor/android-mbedtls/PATCHES.md`.

The resumed TLS runtime matrix passes 133/133 Clang and 132/132 optimized GCC
tests with ASan/LSan/UBSan. It first exposed a separate codec-fixture failure:
provider hash self-tests allocate scratch but had no scoped TLS heap. The test
now owns and retires that heap. An additional registered regression checks
failed self-tests, leftover/denied scratch and successful subsequent execution;
all six affected tests pass on both compilers after its addition. No allocator
restriction or self-test assertion was removed.

A bounded offline TLS fuzz campaign completed 130,042 executions in 301 seconds
with three public certificate-flight seeds and a 20,000-byte input cap, with
no sanitizer finding in that campaign. This is not exhaustive validation.
Normal TLS-OFF safety passes 145/145 Clang and 144/144 optimized GCC tests.
**Full TLS safety still fails**: the unchanged Clang provider analysis reports
findings in seven translation units. No finding has been waived; further
provider review is required before enabling transport. These are Linux host
results, not Android, Windows, macOS, physical custody or real-network results.

The original artifacts below remain intact. A second copy is preserved in
`.cache/android-wallet/continuation-20260912/tls-evidence.tar`. The same directory
contains `existing-work.patch`, `untracked-wallet.tar`, original branch/commit
and status records, and verified `SHA256SUMS`. Together these preserve the
pre-continuation source state, notes, reproducer, corpus, binaries and outputs.

Normal host and Android builds exclude the C transport candidate and its
certificate/TLS provider translation units. The default hash configuration
retains only the pre-existing SHA-256, SHA-512, RIPEMD-160 and self-test profile.
`ZCL_TLS_REVIEW` is OFF by default; explicit host security review is its only
supported use, and CMake rejects combining it with Android or JNI. Whole-archive
symbol inspection checks that normal builds contain no TLS entry points.
There is no JNI/UI network binding or enabled wallet endpoint. Do not remove
this quarantine until the certificate finding and provider analysis below are
resolved and the relevant checks pass. Successful ordinary exchanges do not
override a sanitizer finding.

## Preserved certificate finding

On 2026-09-12, the offline TLS parser fuzzer reported undefined behavior at
Mbed TLS 3.6.7 `library/x509_crt.c:344`, in `x509_name_cmp`. The stack continues
through `x509_crt_check_ee_locally_trusted`, certificate-chain verification,
the TLS client certificate parser and the C integration's handshake function.
The observed operation is `memcmp` with a null first pointer and zero length.
This is a C undefined-behavior finding; no exploitable memory corruption or
certificate-validation bypass has been established.

The provider source at discovery was unchanged from the digest-checked 3.6.7
release. Its file SHA-256 was
`3b484debf0811babaa87450f689e38d7ed60257193c67bbe78f2c89d4226b027`.
The fuzzer uses the actual configured provider and required chain/hostname/time
verification, with a bounded in-memory peer and deterministic test-only TLS
randomness. It has no sockets, wallet keys or production randomness override.

Preserved development artifacts:
`/root/codex-artifacts/zcl-wallet-tls-20260912/` contains the original reproducer,
corpus, source/binary hashes, original provider source, fuzzer binary, discovery
and reproduction logs, provider-analysis reports and an artifact SHA-256 list.
The crash input SHA-256 is
`b551d41b6b4d0112afc9cb4954df670d80fbd593576b724d0b3d2bb2df83546a`.
Its libFuzzer filename is
`crash-0e07970871f9725abbbf0741abcefcf1c641be20`; that suffix is not its SHA-256.
The retained fuzzer binary SHA-256 is
`8bed2042ddd7f6104bb1d2c3dda058d3834793d2cc92add9e5461e8e3e75e95b`.
The original campaign stopped with exit 1; its final source/binary hash checks
matched. Replaying the input with UBSan stack traces reproduced the finding.

The ordinary empty-name certificate fixture currently passes through rejection
without reproducing this exact path. It is not a regression proof for the saved
input. The provider was unpatched at the original checkpoint. A permitted follow-up
must prove a focused before/after regression, retain certificate rejection and
hostname checks, inspect adjacent empty-name/value cases and repeat defensive
parser fuzzing. No sanitizer suppression is an acceptable resolution.

## Static analysis and other evidence

Authored allocator and failed-socket cleanup now make ownership explicit and
pass the previously failing Clang/GCC checks. Provider-wide Clang analysis still
reports findings in `bignum.c`, `bignum_core.c`, `rsa.c`, `ssl_msg.c`, `ssl_tls.c`,
`x509.c` and `x509_crt.c`. Reports include dead stores and paths requiring review
of MPI initialization, provider return-value and TLS-context invariants. The
full TLS/provider safety review therefore remains failing. No warning baseline, checker
disable or source suppression has been added to turn that result green.
The ordinary safety command now states its enabled-wallet scope and analyzes
only enabled translation units. The third argument `ON` explicitly selects the
quarantined host review, including the unchanged provider analysis. That review
was not rerun in this continuation. A passing enabled-wallet result does not
resolve or qualify the disabled TLS candidate.

The C loopback tests pass authenticated ECDSA/RSA exchanges and reject wrong
trust, hostname, time, key usage, weak RSA and unsupported cipher suites. They
also exercise malformed/trailing root DER, record integrity, byte quotas,
timeouts, concurrent cancellation, heap limits and failure cleanup. Allocation
fault tests cover the first 80 allocations plus sampled later allocations;
they do not cover every provider control path. Android debug compilation,
Android lint and 41 JVM/JNI tests passed before the Android transport exclusion;
that exclusion still needs its own build check.

This item remains open and unqualified. If a particular defensive operation is
restricted by the platform, preserve its evidence, mark that operation
**BLOCKED FOR MANUAL SECURITY REVIEW**, and continue other wallet work. Do not
bypass the restriction or enable this transport while the finding is open.
