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
**Full TLS safety still fails**: at that checkpoint Clang provider analysis
reported findings in seven translation units. No finding was waived; further
provider review is required before enabling transport. These are Linux host
results, not Android, Windows, macOS, physical custody or real-network results.

## Resumed MPI temporary-lifetime review

Three overwritten initial values in `bignum_core.c` are removed. The used
constant-time comparisons, range bounds, RNG failures and retry limits remain
unchanged; function complexity remains 2 and 4. Public fixtures check 65,824
comparisons and 14 sampling/error cases. Four mutations are detected. Full TLS
runtime passes 135/135 Clang and 134/134 optimized GCC tests with ASan/LSan/UBSan.
The arithmetic and authenticated-transport fixtures also pass with 32-bit MPI
limbs on the native Linux host; this is not native Android or 32-bit OS evidence.

The whole changed translation unit passes canonical Clang analysis. A fresh
inventory retains ten findings across six other provider units; canonical TLS
safety still stops at the `bignum.c` MPI ownership finding. Additional GCC
whole-unit analysis reports fifteen bounds findings that also reproduce on the
original `bignum_core.c`. Their traces require further invariant review; none
has been suppressed or counted as a passing gate.

A new bounded TLS parser campaign completed 181,309 executions in 301 seconds,
using the same three public seeds, 20,000-byte input limit and 512 MiB RSS limit.
No sanitizer finding occurred in that campaign. This does not resolve static
analysis findings or establish complete provider safety. Android/JNI TLS stays
disabled. Exact patch identity and code-generation review limits are recorded
in the provider's `PATCHES.md`.

## MPI ownership regression

A separate Linux host fixture now preserves the MPI diagnostic coverage as
registered tests: 33,536 small-integer inverse cases, empty/padded input storage,
aliased output, four invalid moduli, failed growth preserving the original
allocation, and all 13 observed inverse-allocation failure sites followed by
successful retries. It checks object invariants and requires normal cleanup to
empty the scoped heap before its emergency-clear path. All inputs are public.

The fixture passes strict Clang/GCC analysis and ASan/LSan/UBSan, including the
32-bit-limb host profile; its largest function complexity is 5. Mutations that
omit cleanup, misreport allocation failure or omit the result copy are detected.
Final full TLS runtime passes 136/136 Clang and 135/135 optimized GCC tests.
Normal-build TLS exclusion still passes. These regressions strengthen ownership
evidence; they do not waive the unresolved provider analysis findings above.

## RSA public-key parser error mapping

Two unused import-result assignments are removed without changing the calls,
error mapping, bounds or key checks. Parser complexity remains 10; optimized
Clang/GCC code matches its respective original `.text` bytes. The independent
RSA uninitialized-buffer analyzer finding remains unresolved.

The new Linux host fixture checks every truncation of a fixed public DER input,
malformed tags/lengths, invalid components, trailing bytes, both import-allocation
failures and complete cleanup. Inputs use exact-size allocations for sanitizer
boundary checks. Both compiler analyzers pass for the fixture; four error-map
and length-check mutations fail. The structural fixture is not a trusted key.

After rebuilding all runtime executables, Clang passes 137/137 and optimized GCC
passes 136/136 TLS tests with ASan/LSan/UBSan. The final focused fixture also
passes the 32-bit-limb host profile. Canonical TLS safety still fails at the MPI
ownership analysis finding; no checker is suppressed. These are host results,
not Android/device qualification. The bounded fuzz campaign above preceded this
RSA source change and is not claimed as a new campaign on this final tree.

## Cleanup-oracle qualification

An isolated mutation removing normal anchor destruction previously passed the
saved-flight regression and LeakSanitizer: production emergency heap cleanup
reclaimed the residual allocation. This exposed a test blind spot, not a leak
in the current production cleanup. Linux host fuzz/seed and saved-flight targets
now observe the existing heap-clear boundary and require normal provider cleanup
to have emptied it. Production emergency reclamation remains unchanged.

The cleanup-removal mutation now fails at that observer. A registered probe
checks normal cleanup and a residual allocation in a bounded child; removing
observer wiring makes this probe fail. Clang/GCC analysis passes, and new C
functions have complexity at most 6. Normal wallet compilation excludes the
observer, which is not Android transport enablement.

Full TLS runtime passes 138/138 Clang and 137/137 optimized GCC tests. A fresh
offline parser campaign with the observer completed 139,977 executions in 301
seconds without a sanitizer/cleanup finding, with the three public seeds,
20,000-byte input cap and 512 MiB RSS cap. These remain bounded host results;
the unresolved provider analysis gate still blocks TLS qualification.

## MPI multiplication bounds oracle

A host-only fixture compares the unchanged multiply-accumulate primitive with
OpenSSL public-integer arithmetic in 640 bounded cases. Exact-size allocations
cover zero/one-limb inputs and both sides of the eight-limb loop boundaries,
destination extensions, maximal carry, source preservation and documented exact
aliasing. Explicit byte conversion supports both 32-bit and 64-bit MPI limbs.

Clang and optimized GCC ASan/LSan/UBSan runs pass, as does the 32-bit-limb Linux
host profile. Rounding the eight-limb loop upward causes an ASan heap overrun;
discarding the carry fails the independent arithmetic assertion. Fixture static
analysis passes on both compilers, and changed/new functions have complexity
at most 10. No provider algorithm or Android transport configuration changes.
Rebuilt full TLS runtime suites pass 139/139 Clang and 138/138 optimized GCC
tests with ASan/LSan/UBSan. These results strengthen bounded runtime evidence
without waiving the existing GCC analyzer bounds findings or the canonical TLS
analysis failure.

## Concurrent allocator ownership

A Linux host regression now overlaps two independent TLS heaps. Barriers make
scope entry/exit ordering explicit; each worker owns its heap and blocks until
cleanup completes. Thirty-two allocation rounds check zero initialization,
preserved bytes, accounting, nested-scope refusal and allocation denial isolated
to one worker. Leaving one scope cannot disable the other or permit unscoped
allocation. No production allocator or transport code changes in this slice.

Replacing thread-local scope with shared scope in an isolated source copy passes
the prior serial transport fixture but fails the new ownership assertion. The
new test and existing transport test pass Clang/GCC ASan/LSan/UBSan. The separate
ThreadSanitizer build passes transport, allocation-failure and concurrent-heap
tests (3/3). Replacing cancellation's atomic write with a non-atomic write in a
private mutation produces TSan's expected data-race report and exit 66; the
unchanged atomic implementation passes. This verifies the observed C paths,
not all interleavings or the internals of system OpenSSL used by the fixture.

The fixture passes both compiler analyzers with maximum complexity 7. The
normal-build TLS quarantine and registered test deadlines still pass. These
focused additions follow the full runtime matrix above; a full TSan suite,
Android TLS, physical devices and native Windows/macOS are not claimed.

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

## Original quarantine checkpoint: static analysis and other evidence

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
