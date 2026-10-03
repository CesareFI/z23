# Development record

## Explicit historical funding-source inspection — 2026-09-17

`zcl_legacy_source_inspect` now reads v1/v2/Overwinter v3 through a separate C
profile under the original100000-byte bound. Shared transparent parsing, checked
amounts, full-wire SHA256d, owned fields and retirement avoid a parallel reader.
The common view preserves the existing v4 alias/layout. Current v4 cap/admission,
draft, review, commitment, JNI and signing behavior stay unchanged. Unknown
formats refuse; absent expiry/Sapling fields remain zero.

The pinned original transaction/PHGR/consensus sources were inspected as Git
objects and SHA256-recorded. PHGR is not just opaque296-byte storage: its seven
G1 and one G2 encodings require exact leading-byte masks. All256 values at every
proof position are exercised, alongside all fixture truncations, exact IDs,
selected output bounds, version/group mismatch, extra bytes and failure canaries.
Proofs, signatures, scripts and historical consensus validity remain unverified.

The reproducible projection tool verifies original JSON hashes, refuses an
existing output directory and never executes a reference node/wallet. Four
committed v1/v3 fixtures are untouched originals; three v2 fixtures explicitly
replace the v3 header and remove only group/expiry fields. PHGR/signature bytes
remain unchanged. Independent OpenSSL SHA256d pins every complete wire identity.
The generator reproduces the header byte-for-byte, including after an existing-
directory refusal. These are public serialization fixtures, not spendability
or original-node acceptance evidence.

Seven group/prefix/length/cap/hash/publication/retirement mutations fail. Dirty
provider failures retire both hash buffers and staging for every fixture.
OpenSSL hash fuzzing completes1734915 runs in61 seconds without a finding.
The isolated `708e6205e` snapshot passes TLS-off analysis/complexity,135 Clang
sanitizer/oracle and130 optimized GCC sanitizer groups. The strengthened prefix
fixture and v4/commitment regressions pass final six-group Clang/GCC reruns.
Eight final source/tool files are SHA256-recorded. Shared optimized inspector
frames280/352 bytes remain below4096. Android/JVM, debug/minified release, lint,
fixture/result isolation and16KiB gates pass (155 tasks). Final normal/fault
release-archive fixtures pass API30/35/36 x86_64 with owned temporary-path cleanup;
ARM64 compiles/alignment only. Architecture/docs gates pass. Evidence:
`.cache/android-wallet/mission-20260917/legacy-source/` and
`.cache/android-wallet/mission-20260917/historical-source-audit/`.

Next: compose historical funding identity/commitments and mixed-format offline
assessment through an explicit profile, retaining the64-byte preimage refusal
and exact source matching. Keep existing v4/spend/review profiles intact until
that path is separately qualified. Chain/unspentness evidence, physical custody,
shielded spending and quarantined TLS remain open; no live endpoint is enabled.

## Exact source/outpoint/header commitment composition — 2026-09-17

The internal C `zcl_v4_source_commitment_check` now composes existing header,
full-v4 source and bounded Merkle owners. It matches explicit header identity,
then exact source identity/selected output, then the path against that header's
transaction root. Only a complete owned source/header result publishes; staging
retires on success and every entered failure. Expected IDs, height/count/index
remain caller claims. No chain validity, freshness, unspentness, custody, consent,
JNI/UI send or TLS authority is added.

The composition explicitly refuses64-byte source preimages, the same input size
as internal Merkle-node hashing. A deterministic64-byte envelope passes the
structural source reader and is refused here. It is deliberately not a valid
funding transaction. This conservative profile guard changes no node consensus
predicate and makes no off-path uniqueness or accepted-chain claim.

All eight opaque v4 section profiles, both selected outputs, one through32-level
paths, UINT32_MAX tree width, both header networks, identity/source/branch
mutations, bad indices/lengths/nulls and complete failure-output preservation
pass. Dirty component failures qualify exact stage ordering, no later work,
complete staging retirement and early no-work refusals. Seven deliberate
identity/index/path/length/publication/wipe mutations fail. OpenSSL-backed fixture
commitments drive256720 fuzz executions in61 seconds without a finding; the
source parser remains the structural verdict owner in that composition harness.

The isolated `c6259d727` snapshot passes TLS-off analysis/complexity,133 Clang
sanitizer/oracle and128 optimized GCC sanitizer groups. A final fixture-only
padding-copy portability adjustment passes focused Clang/GCC reruns and all seven
mutations again; the eight final native files are SHA256-recorded. Optimized
entry frames360/384 bytes remain below4096. Android/JVM, debug/minified release,
lint, fixture/result isolation and16KiB gates pass (155 tasks). Final normal/fault
fixtures linked against release-built archives pass API30/35/36 x86_64, with
owned temporary paths removed; ARM64 compiles/alignment only. Architecture/docs
gates pass. Evidence:
`.cache/android-wallet/mission-20260917/source-commitment/`.

Next: audit original v1/v2/v3 historical funding layouts and untouched public
fixtures, then extend source coverage through an explicit profile while keeping
existing v4/spend/review admission intact. Accepted-chain/unspentness evidence,
physical custody, shielded spending and quarantined TLS remain separate gates.

## Owned raw-header inspection — 2026-09-17

The existing RPC header shape/hash logic now has one raw C owner returning
complete owned identity, previous/Merkle/Sapling roots, nonce and scalar fields.
All uint256 fields use display order. Canonical solution length/prefix and both
Bubbles schedules are preserved; arbitrary scalar bits and the old internal
uint32 height domain remain accepted. Public tip height keeps its signed32-bit
gate. No PoW, chain, inclusion, freshness, unspentness or send authority is added.
The RPC adapter clears its decoded header/view on success and refusal, including
partial hex decoding; raw work retires after success and either hash failure.

Original `14a83d510` header/chainparams objects are SHA256-recorded. Both real
genesis hashes/fields, every truncated length, extra/SIZE_MAX lengths, malformed
prefixes, network/fork boundaries, scalar bit patterns, unchanged failure output
and destroyed borrowed input pass. OpenSSL independently hashes complete wire.
Eight field/order/epoch/prefix/hash/provider/wipe mutations are caught; differential
fuzzing completes9971739 executions in61 seconds without a finding.

Initial test setup exposed a missing JSON include dependency and a fixture using
a top-level string where RPC requires an object. Both were corrected. The first
focused selection also included three not-yet-built executables; all six focused
groups pass after explicit builds. Original failed logs remain, with no skipped
assertion or weakened acceptance. No production behavior defect was discovered.

The isolated `500a520b2` snapshot plus nine SHA256-recorded native files passes
TLS-off Clang/GCC analysis, complexity limits10/15,131 Clang sanitizer/oracle and
126 optimized GCC sanitizer groups. Optimized raw frames296/304 bytes and RPC
wrapper1752/1776 bytes remain below4096. Android/JVM, debug/minified release,
lint, fixture/result isolation and16KiB checks pass (155 tasks). Normal, dirty-
provider and existing Electrum fixtures linked to release-built archives pass
API30/35/36 x86_64; all owned device temporary paths are removed. ARM64 compiles
with recorded hashes/alignment only. Architecture/docs gates pass. Evidence:
`.cache/android-wallet/mission-20260917/header-inspection/`.

Next: compose exact full-v4 source/outpoint/header commitments, reviewing
leaf-versus-internal-node ambiguity explicitly. Caller-supplied roots/counts and
matching bytes must never promote state to chain-accepted or unspent. Physical
custody, legacy funding coverage, shielded spending and TLS gates remain open.

## Bounded transaction Merkle-path consistency — 2026-09-17

The internal C `zcl_merkle_branch_check` now checks a transaction-ID path against
an explicit root and claimed uint32 tree width/index. Display-order hashes convert
to original raw uint256 order before ordered SHA256d pairs. Exact depth, valid
index, odd-width last-node self-copy and unequal actual siblings on the path are
required. Work is at most32 pairs/64 hashes, without allocation or retained state.
This establishes conditional hash-path consistency only. Count/root authority,
leaf-versus-internal-node meaning, off-path uniqueness, chain acceptance,
freshness, maturity and unspentness remain unproven. No JNI, send or node predicate
changes; TLS and custody gates remain intact.

Pinned original `14a83d510` block/partial-Merkle/uint256 sources were inspected as
Git objects and SHA256-recorded. Its partial extractor uses a200000-byte block
constant and /60 count guard; this bounded helper deliberately introduces no
block-size/height validity rule. The path checks follow the original byte order,
ordering, odd-width behavior and partial-extractor equal-sibling refusal.

Independent full-tree fixtures exercise every leaf for widths1..65, boundaries
around every uint32 power of two and a32-level UINT32_MAX path with synthetic
subtree commitments. OpenSSL-generated roots reproduce exactly. All64 dirty
hash-provider failure positions and complete scratch retirement pass. Nine
byte-order/depth/width/duplication/side/hash/root/wipe/provider mutations fail.
The mutation generator initially expanded awk replacement ampersands into invalid
C; direct line assignment corrected that harness error before the full rerun.
OpenSSL differential fuzzing completes224959 executions in61 seconds, no finding.

The isolated `87f19cbe7` snapshot plus ten SHA256-recorded native files passes
TLS-off analysis/complexity,129 Clang sanitizer/oracle and124 optimized GCC
sanitizer groups. Optimized Clang/GCC frames measure248/272 bytes. Android/JVM,
debug/minified release, lint, fixture/result isolation and16KiB gates pass
(155 tasks). Normal and dirty-provider fixtures linked to release-built archives
pass API30/35/36 x86_64 with owned temporary-path cleanup; ARM64 builds only.
Evidence: `.cache/android-wallet/mission-20260917/transaction-merkle/`.

Next: factor the existing serialized-header checks into an owned raw-header
inspection result, pin hash/previous/Merkle byte order against original genesis
fixtures, and compose exact source/outpoint/header commitment checks. Keep
structural/hash evidence distinct from authenticated chain/unspentness and send.

## Full-source review foreground and restart qualification — 2026-09-17

The existing display, rendering-failure, Activity-lifecycle and process-relaunch
fixtures now accept an explicit instrumentation-only full-source profile. A
shared public fixture factory uses `prepareFullSources` for that profile and
retains the original narrow open/prepare routes otherwise. Expected transaction
identity comes from independent Java SHA256d of committed expected wire. Unknown
profiles refuse. No production UI, native code, wallet or endpoint changes.

`check-process-relaunch.sh ... review-full` selects this preparation profile and
requires its matching readiness/profile/PID before terminating the exact public
fixture process. Relaunch uses the existing strict result checker, including
test identity/completion and skip rejection. Invalid profile/non-emulator serial
controls refuse before report-directory creation or adb invocation.

The isolated `6df4e50bf` snapshot passes all155 Android/JVM/debug/minified-release/
lint/fixture/result/16KiB gates. Six changed fixture/controller files and both
debug APKs are SHA256-recorded. On each API30/35/36 x86_64, both profiles pass14
display/render/lifecycle tests without skips (28 per API), including exact trust
labels, queued delivery, expiry, rendering failure, repeated recreation, late
prepared-owner refusal and background cleanup. Each API also passes both opt-in
process-relaunch profiles: verified public display, exact PID termination, new
PID, empty view and a reusable native review slot with no automatic replay.

The existing software-emulated API36 instance timed out at the outer90-second
limit during legacy recreation; its activity-service diagnostic also timed out.
Those records remain intact. A fresh isolated hardware-accelerated API36 AVD
completed startup after the initial short readiness window and passed the same
assertions/timeouts. Existing emulators were preserved. API30/35 use5558/5560;
the qualified fresh API36 uses5562. Evidence:
`.cache/android-wallet/mission-20260917/full-review-lifecycle/`.
ARM64 and minified release remain build-only evidence for this slice; physical
custody, authenticated chain/unspentness and send remain separate open gates.

Next: audit pinned original Merkle behavior and header/source hash byte order,
then establish bounded reference-backed transaction commitment/inclusion checks.
State the exact proof claim; a branch/root match alone must not become chain
validity, freshness, unspentness or signing authority. TLS remains quarantined.

## Single-call full-source preparation — 2026-09-17

`UnsignedReview.prepareFullSources` now constructs and opens an offline unsigned
review through one JNI call under the existing process owner lock. It reuses the
bounded source-copy owner and existing parameter/destination parsers. Both C
phases see the same allocation; no intermediate draft returns to Java. Java refs
retire before construction, request descriptors and draft wire clear before
source metadata and bytes wipe/free. Legacy preparation stays narrow; this adds
no send action, proof verification, authenticated chain state or custody authority.

Native fixtures exercise all source/parameter/destination read failures, partial
writes, exceptional non-null refs, max rows, dirty construction, shared ownership
and replacement. A hook destroys Java source bytes after construction; opening
still matches the original C-owned wire. The expanded wipe observer initially
confused two equal-sized objects; parameter-pointer identity fixes that harness
error. A BUSY-guard mutation initially survived because the inner owner still
refused replacement; a stronger regression now checks zero preparation work on
BUSY. All eight routing/admission/index/cleanup/failure-gating mutations fail.
Initial fuzzing completes25911 executions; final ownership/lifecycle fuzzing
completes18162 in46 seconds, neither with a finding.

The isolated `2e4fdf020` snapshot passes TLS-off static analysis/complexity,
123 Clang and122 optimized GCC sanitizer groups. The independent-oracle Clang
profile passes127 groups; final observer checks also pass focused GCC.
Optimized Clang/GCC preparation frames measure3144/3248 bytes, below4096.
Five new JVM tests pass under `-Xcheck:jni`; the155-task Android/JVM/debug/
minified-release/lint/fixture/result/16KiB gates pass. Eleven debug instrumentation
tests (three preparation, three full opening, five legacy) pass without skips
on each API30/35/36 x86_64 emulator against recorded APK hashes. ARM64 is build
evidence only. Thirteen source/test files are SHA256-recorded. Evidence:
`.cache/android-wallet/mission-20260917/jni-full-prepare/`.

Next: qualify this preparation path through foreground presentation, delayed
delivery, background cancellation, recreation and explicit process relaunch.
Keep authenticated-chain/unspentness, hardware custody and send gates open.

## Explicit full-source draft construction — 2026-09-17

The internal `zcl_transaction_draft_full_sources` now constructs a bounded
transparent unsigned transaction from exact selected outputs of full v4 sources.
It derives each complete source identity, retains explicit row/sequence/output
order, then reuses full-source amount/destination/fee assessment before publishing
the whole owned transaction. The original public builder remains narrow. Source
proofs/signatures remain opaque: changing those bytes selects a different
unverified outpoint, not authenticated chain evidence. No JNI preparation, send,
custody, consent or TLS authority is added.

Fixtures cover both networks/all conditional tails, shared sources with distinct
indexes, duplicate-outpoint refusal, mixed narrow/full sources, exact wire after
destroying borrowed data, every source truncation and unchanged output on failure.
The existing complete draft contract and dirty-provider retirement fixture also
run through the new profile. Eight routing/index/later-error/assessment/network/
cleanup mutations fail deterministic regressions. Full-source construction
fuzzing completes 85,420 executions in 46 seconds without a finding.

The isolated `908fbbf44` snapshot plus seven SHA256-recorded native files passes
TLS-off analysis/complexity, 127 Clang sanitizer/oracle and 122 GCC sanitizer
groups. Optimized Clang/GCC frames measure 2264/2288 bytes for construction,
168/192 for full-source binding and 1224/1232 for assessment. Android/JVM,
debug/minified release, lint, fixture/result isolation and 16KiB gates pass
(155 tasks). Normal and dirty-provider fixtures linked against release-built
archives pass API30/35/36 x86_64 with owned temporary-path cleanup. ARM64 builds
only. Evidence: `.cache/android-wallet/mission-20260917/source-draft/`.

Next: one JNI operation that constructs and opens a full-source review using
the same bounded source copy, with real-VM and lifecycle qualification. Hardware
custody, authenticated chain/unspentness and send gates remain open.

## Bounded JNI full-source review — 2026-09-17

The managed `UnsignedReview.openFullSources` operation now reaches the explicit
C full-source review lifecycle. JNI captures all source refs/lengths first,
checks1..8 rows of1..102000 bytes and total<=816000, then allocates exactly the
sum. Java refs retire before C preparation. Draft and pointer metadata clear
before the source allocation wipes/frees, including partial-copy exceptions,
allocation failure and C refusal. Existing opening/preparation stay narrow;
both profiles share the same mutex, owner and replacement/expiry rules. No
new send action, chain evidence, key access, consent or TLS authority is added.

The native fake-VM fixture now exercises both profiles, exact allocation size,
all length/element/region failures, non-null references with pending exceptions,
reference replacement during allocation, maximum816000-byte copying and cleanup
order. An entry observer proves no Java references remain before C preparation
and all source spans belong to the one allocation. Initial fixture failures
identified legacy-only entry selection, fixed allocation assumptions and stale
per-call counters in the expanded harness; those were corrected without changing
product refusals or assertions. Eight allocation/cap/index/exception/ref/cleanup
mutations fail. Final JNI fuzzing with the entry observer completes18163
executions in46 seconds; an earlier run completed27812, neither with a finding.

The isolated `c08834da4` snapshot passes TLS-off static analysis/complexity,
125 Clang sanitizer/oracle and120 optimized GCC sanitizer groups. The final
ownership observer passes focused Clang/GCC gates and complexity. Fourteen
final source/resource files are SHA256-recorded. The new adapter's optimized
frames measure2296 bytes with Clang and2400 with GCC. Four JVM tests pass under
`-Xcheck:jni`; the155-task Android/JVM/debug/release/minification/lint/fixture/
16KiB gates pass and are rechecked after final test qualification. Eight real
instrumentation tests (three full-source, five legacy) pass without skips on
each API30/35/36 x86_64 emulator against SHA256-recorded final debug APKs.
Minified release builds only; ARM64 builds only. Public C-generated fixture
bytes reproduce exactly with `seed_full_review`. Evidence:
`.cache/android-wallet/mission-20260917/jni-full-review/`.

Next: explicit full-source draft construction from chosen funding/outputs,
followed by foreground/UI lifecycle qualification. Keep hardware custody,
authenticated chain/unspentness and send gates open. TLS stays quarantined.

## Owned full-source review lifecycle — 2026-09-17

An explicit internal opening operation now composes full-v4 source assessment
with the existing owned unsigned-review lifecycle. Preparation still validates
the current transparent profile, requires empty input scripts and serializes
only the owned parsed value. No source pointer/wire survives opening. Failed
preparation preserves owner/ID; success retains the same fixed lifetime,
rollback cancellation and non-repeating replacement identity. Legacy opening
keeps its original source profile. No JNI/UI exposure or signing prerequisite
changes; source proofs/signatures remain opaque and chain state unverified.

New fixtures cover both networks/all conditional tails, every source truncation,
destroyed borrowed sources, exact retained wire/context, expiry equality,
rollback and cross-profile replacement. The entire existing review contract
also runs through the new entry point. Dirty parser, assessment and serializer
fixtures exercise both modes and observe parsed/candidate retirement. Seven
mutations of profile routing, unsigned-input refusal, publication, both clears
and serialization gating fail. The mutation generator initially matched two
candidate clears and later produced misleading indentation; its scope/format
were corrected and the intended single mutations rerun. Product code and
assertions did not change to accommodate those harness errors.

An isolated `d33dba05d` snapshot plus seven SHA256-recorded native files passes
TLS-off analysis/complexity,125 Clang sanitizer/oracle and120 GCC sanitizer
groups. Differential full-source lifetime/context/sighash fuzzing completes
176734 executions in46 seconds with the independent oracle and no finding.
New/shared optimized open frames measure3416/3424 bytes, preparation2264/2288
with Clang/GCC; no new heap or attacker-sized automatic object. Android/JVM,
debug/release, both lints, fixture/result isolation and16KiB alignment pass
(155 tasks). Normal and interposed retirement fixtures linked to release
archives pass API30/35/36 with owned temporary-path cleanup; ARM64 builds only.
Evidence: `.cache/android-wallet/mission-20260917/source-review/`.

Next: bounded JNI source ownership and real-VM lifecycle qualification before
Android exposure. Keep the physical custody and authenticated-chain gates open;
TLS stays quarantined. Public fixtures only, no real funds or private state.

## Explicit full-source offline assessment — 2026-09-17

Full raw source identity/index matching now feeds a separate internal offline
assessment entry point. It reuses existing destination, amount, fee and current
transaction checks. Legacy callers select their original source path internally;
they accept no new mode argument. No draft/review/signing/JNI authority changes.
Every report is complete-only, and sources remain caller-owned through the
synchronous call. At most eight sources of102000 bytes are inspected sequentially
without allocation or retained pointers. This is byte consistency and arithmetic,
not proof validation, inclusion, unspentness or shielded spending.

The entire existing assessment contract also runs through the new entry point.
New fixtures cover all conditional source tails, mixed profiles, independently
pinned original identities, exact index/script selection, signature-byte changes
and legacy refusal. Both assessment modes share dirty-provider cleanup tests;
matching adds nested candidate retirement and unchanged-output checks. Seven
mutations of identity, index, publication, cleanup, legacy dispatch, later failure
and fee enforcement fail. Reference and positive synthetic-source fuzz corpora
complete375961 and722805 executions respectively (46 seconds each), without a
finding. Synthetic proofs/signatures are deliberately invalid and never claimed
as chain evidence.

An isolated `9956829fd` snapshot plus ten SHA256-recorded files passes TLS-off
static analysis/complexity,123 Clang sanitizer/oracle and118 optimized GCC
sanitizer groups. New matching frames measure152/192 bytes and shared assessment
frames1224/1248 bytes with Clang/GCC. Android/JVM, debug/release, both lints,
fixture/result isolation and16KiB alignment pass (155 tasks). Actual release
archive fixtures pass API30/35/36 with invocation-owned path cleanup; ARM64
compiles only. Evidence:
`.cache/android-wallet/mission-20260917/source-assessment/`.

Next: bounded JNI source ownership and an explicit offline-data lifecycle before
Android exposure. Preserve hardware custody, authenticated-chain and signing
admission gates. TLS remains quarantined; no real funds or private fixtures.

## Full v4 public source inspection — 2026-09-17

The existing transparent funding codec cannot inspect a funding transaction
containing shielded components. A separate internal C inspector now provides
bounded parsing/identity: complete canonical v4 layout, opaque Sapling/Groth
descriptions and signatures, full-wire SHA256d, selected owned output and raw
metadata. Layout and the 102,000-byte bound come from pinned original Zclassic.
Existing prevout/draft/JNI admission remains unchanged. This adds no consensus,
chain, unspentness, signing or shielded-spend authority.

Four untouched original wires reproduce from a hash-pinned reference with
independent OpenSSL identities. Every truncation and byte change, conditional
tails, extended counts, exact/over-limit wire, noncanonical CompactSize and
dirty hash-provider failures are covered. Six mutations removing canonicality,
exact end, either signature tail, hash reversal or candidate retirement fail
deterministic regressions. Fuzzing completes 333066 executions in 46 seconds
without a finding. An initial projection fixture exposed invalid expiry in an
original hash vector; only the separate transparent projection was corrected,
preserving reference bytes and production predicates. A missing direct
hash-provider include dependency in the fault target was corrected before
final validation.

An isolated `1921c7b31` snapshot plus eight SHA256-recorded files passes TLS-off
static analysis/complexity, 121 Clang sanitizer/oracle and 116 optimized GCC
sanitizer groups. The final added noncanonical-count regression also passes
both profiles and test complexity. Production frames measure 312 bytes with
Clang and 368 with GCC, without heap allocation. Android/JVM, debug/release,
both lints, fixture/result isolation and 16 KiB alignment pass (155 tasks).
Release-library fixtures pass API30/35/36 with owned temporary-path cleanup;
ARM64 compiles only. Evidence:
`.cache/android-wallet/mission-20260917/v4-funding/`.

Next: qualify explicit funding-source admission and bounded JNI ownership before
lifting the old source profile. Hardware custody and authenticated chain state
remain open; TLS stays quarantined. No real funds or private fixtures were used.

## Reviewed output change ownership — 2026-09-17

The existing input-ownership checks now also support an internal exact-output
change predicate. It selects only the live review's output row, requires a
chain1 P2PKH claim, and reuses exact committed-wallet/recovered-entropy and
consumed-index matching. It samples trusted monotonic time before admission and
after ownership work/secret cleanup. Receive paths, wrong wallets, unconsumed
indexes, recipients and stale completion refuse. Nothing is reserved or written;
output amounts/destinations remain visible review data, not inferred intent.

Normal fixtures cover both networks/all entropy lengths, sixteen outputs,
destroyed borrowed draft sources, unchanged journals and both clock phases.
The existing source-copy failure fixture now proves claim copying and complete
secret cleanup before the final output-check clock, including dirty provider
refusal. The independent claim fuzzer also models output ownership and time.
Four mutations fail; fuzzing completes3588 executions in47 seconds without a
finding. The first fixture incorrectly expected OUT_OF_RANGE for an unconsumed
valid index; it was corrected to the existing NOT_FOUND contract without changing
the refusal. The expanded test helper initially exceeded complexity15 and was
split before final validation. Production complexity remains at most10.

Validation used an isolated `55a17c2ea` snapshot plus six native files recorded
by SHA256. Concurrent TLS/provider edits were preserved and excluded, including
their separate CMake hunk. The canonical TLS-off gate passes all static analysis
and complexity checks, 119 Clang sanitizer/oracle groups and 114 optimized GCC
sanitizer groups. The new optimized frame is1416 bytes with Clang; GCC uses an
80-byte gate plus the separate1408-byte wallet-check frame. Android/JVM,
debug/release, lint, fixture/result isolation and16KiB alignment pass (155 tasks).
Release-library x86_64 fixtures pass API30/35/36 with owned temporary-path cleanup;
ARM64 compiles only. Evidence: `.cache/android-wallet/mission-20260917/review-change/`.

This supplies no JNI/UI entry point, hardware authentication, output hiding,
consent, journal recency, chain/unspentness or broadcast authority. Those
prerequisites remain open before send. TLS stays quarantined. Continue composing
the exact reviewed payment/change intent with these ownership primitives and
qualifying custody/lifecycle boundaries; preserve the physical-device gate.

## Recovered change seed last-use retirement — 2026-09-17

The recovered-change composition previously retained its receive-address binding
buffer through change derivation and its seed through final context release and
output publication. The existing secret-provider fixture now observes those
objects' live identities, requires exact object clears and checks cleanup order.
It reproduces the old lifetime before the fix. Four production lines clear the
35-byte anchor immediately after its binding decision and the 64-byte seed
after its last derivation, before publication/context release. The complete
work-object clear remains. Derivation, separate blinding contexts, output bytes
and failure atomicity are unchanged; supplied entropy remains caller-owned.

The focused normal/key/provider suites pass. Removing either new clear in a
source-copy mutation fails the strengthened fixture. An isolated snapshot of
`45040b9b0` plus three SHA256-recorded native files passes the complete TLS-off
gate: static analysis/complexity, 118 Clang sanitizer/oracle groups and 113
optimized GCC sanitizer groups. The existing recovered-change differential
fuzzer completes 829 executions in 46 seconds without a finding. Optimized
frames measure 280 bytes with Clang and 352 bounded bytes with GCC.

Android/JVM, debug/release, lint, fixture/result isolation and 16 KiB alignment
pass (155 tasks). Both normal binding and secret-provider fault fixtures linked
against release-built x86_64 archives pass API30/35/36, including observed
retirement order. ARM64 compiles only; this is no hardware-custody proof. Only
public deterministic fixtures were used and invocation-owned executables were
removed. Evidence: `.cache/android-wallet/mission-20260917/change-seed-retirement/`.
TLS remains quarantined. Next: continue custody/lifecycle and complete-send
prerequisite review; hardware qualification and authenticated chain state remain
open before any user-visible signing or real funds.

## Complete internal transaction signing — 2026-09-17

The internal C signer now completes a whole reviewed P2PKH transaction without
exposing intermediate detached signatures. It checks the exact claim count and
every input's wallet ownership before signing, stages each signature privately,
then uses the existing independent verification/completion-time publisher.
Copied claim pointers retire before public assembly and complete staging clears
on success or failure. A later input, provider, clock or final assembly refusal
preserves caller wire and length. No storage, reservation, key path, consensus,
JNI, consent or broadcast authority changes.

Deterministic fixtures cover both networks/all entropy lengths, receive and
consumed change, exact signed bytes, unchanged journals, capacity/count bounds,
all eleven clock refusal/expiry positions and eight real signed inputs. Fault
fixtures exercise each preflight/dirty signer position for counts 1..8 and
observe claim/signature retirement. Five source-copy mutations fail. The
extended claim/completion fuzzer runs 3969 executions in 46 seconds without a
finding. Strict analysis and complexity checks pass; optimized new frames are
1496 bytes with Clang and 1616 bytes with GCC, below the per-frame 4096 cap.

Validation used an isolated copy of `29da3d186` plus seven native files recorded
by SHA256. The shared branch advanced externally during the work; unrelated
Tor state was preserved. The canonical TLS-off safety script passes 118 Clang
sanitizer/oracle and 113 optimized GCC sanitizer groups. Android/JVM checks,
debug/release builds, both lints, fixture/result isolation and 16 KiB alignment
pass (155 tasks). The fixture linked against release-built x86_64 libraries
passes API30/35/36; invocation-owned executables/directories were removed.
ARM64 builds only. Review caught an initial maximum-input fixture reference to
a nonexistent wire-length field; it was corrected before these gates without
changing production behavior. Evidence:
`.cache/android-wallet/mission-20260917/transaction-signing/`.

This remains internal with no JNI/UI entry point. Per-use hardware custody,
explicit exact-review consent, independently authenticated chain/unspentness,
and delayed delivery/broadcast checks remain open prerequisites. TLS stays
quarantined. Continue the highest-value custody/lifecycle review while physical
hardware qualification remains unavailable; do not connect a real send path.

## Review-bound wallet input signing — 2026-09-17

The C core now has an internal synchronous composition that signs exactly one
P2PKH input from a live review. It accepts no caller digest, script, amount or
arbitrary key path. It checks supplied candidate branch/finality/expiry context,
matches the input to the exact committed receive0 wallet or an already consumed
change index, derives the existing BIP44 key, signs the review-owned digest and
independently verifies the public signature/key hash before publication. Trusted
monotonic time is sampled before admission, before ECDSA and after verification;
expiry, rollback or clock/provider failure leaves output unchanged.

Entropy, seed, EC blinding, chain code, scalar, transient context, verification
script and complete work object have explicit last-use cleanup. The fault fixture
injects dirty seed/key/signature/script results, mismatched key/digest, malformed
successful length, EC allocation, RNG and clock failures while checking cleanup
order and output atomicity. Normal fixtures cover both networks, all supported
entropy lengths, receive and consumed-change paths, deterministic repeat output,
unchanged journals and complete independently verified signed wire. The wallet
claim fuzzer now models all three signing clock phases as well as malformed
record/entropy/directory/journal inputs. Strict static analysis and complexity
checks pass. The measured signing frames are 1896 bytes with Clang and 1936
with GCC; these are per-function observations, not total nested stack bounds.

Final validation used an isolated copy of backed-up
`27d0808c64ba85183d291f1cdfabba88aa38640d` plus the nine owned native files,
recorded by SHA256. Concurrent change-state edits were preserved and excluded.
The TLS-off safety script passes all authored/provider analysis and complexity
checks, 116 Clang sanitizer/oracle groups and 111 optimized GCC sanitizer groups.
The claim/signing fuzzer completes 4,983 executions in 46 seconds without a
finding. Eight source-copy mutations ignore ownership refusal, omit either
signing time check, skip public verification or omit entropy/seed/chain-code/
scalar retirement; all fail deterministic regressions. An initial mutation-only
compile refusal for newly unused symbols was resolved by preserving harmless
references, retaining warnings as errors. The initial fixture used a nonexistent
decoded-index snapshot field; it now checks the real exact journal bytes/length.

Android/JVM checks, both ABI debug/release builds, both lints, fixture isolation,
instrumentation-result controls and 16 KiB alignment pass from that isolated
copy. The signing fixture linked against its release-built x86_64 archives passes
API30/35/36, using only owned temporary paths and published entropy/inert wallet
records. Invocation-created executables/directories are removed. ARM64 builds;
physical ARM64 execution remains unproven. Evidence is under the wallet directory
at `.cache/android-wallet/mission-20260917/review-wallet-signing/`.

This has no JNI or UI entry point and does not itself establish hardware-backed
authentication, explicit consent, authenticated chain/unspentness, delayed UI
freshness or broadcast authority. Those remain required before a real send.
TLS is unchanged and quarantined. Next: qualify those external prerequisites
before connecting this foundation to a user-visible send or broadcast path.

## Change workflow scratch retirement — 2026-09-17

The authenticated change create/reserve/reconstruct/recover compositions now
retire their complete local workflow objects. This includes the 80-byte initial,
next, expected and replacement records; complete observed storage/recovery
snapshots; the staged reservation; and the 35-byte reconstructed address.
Cleanup follows the final storage/provider use or conditional output copy, so
success bytes, index-consumption rules, repair behavior and failure atomicity do
not change.

The three source-copy fault fixtures now distinguish and require exact cleanup
for the 35-, 44-, 80-, 96- and 184-byte objects alongside the existing custody
and blinding spans. They cover invalid arguments, every relevant RNG/codec
failure, malformed and partial journals, competing observations, append/repair
failure and successful publication. Focused normal/failure/crash suites pass
under Clang ASan/UBSan and optimized GCC, and focused static analysis is clean.
Optimized Clang frames are 280 bytes for create, 440 for reserve, 376 for
consumed-address reconstruction and 600 for recovery.

The final combined TLS-off gate passes all authored static analysis and
complexity checks, 112/112 Clang tests and 111/111 optimized GCC tests.
Android/JVM checks, ARM64/x86-64 debug and release native builds, lint, fixture
isolation, complete instrumentation-result controls and both-ABI 16 KiB
alignment pass. There is no API, serialization, consensus, JNI or TLS change;
TLS remains quarantined. Next: continue into the highest-value unretired
wallet/custody scratch while the signer remains internal and unexposed.

## Change-state authenticated scratch retirement — 2026-09-17

Inspection found two avoidable stack remnants in the authenticated change-index
path: encoding returned with its complete 80-byte staged record intact, and key
derivation returned with its 35-byte recovered-address anchor intact, including
provider refusal. Encoding now clears its candidate after the conditional caller
copy. Key derivation clears the anchor immediately after the custody binding
check. Existing output atomicity, exact record bytes, KDF inputs and recovery
rules are unchanged.

The source-copy failure fixture now distinguishes and requires exact 35-, 64-
and 80-byte cleanup. It passes success, every extract/expand/record-MAC failure,
tag mismatch, malformed records and malformed wallet headers while proving
failed caller outputs remain untouched. Focused Clang ASan/UBSan and optimized
GCC sanitizer suites pass; focused Clang and GCC static analysis is clean.
Optimized frames are 152 bytes for encode, 168 for decode and 296 bytes for key
derivation. There is no heap, recursion, API, dependency, serialization,
consensus, JNI or TLS change. The full TLS-off native gate passes all static
analysis and complexity checks plus 110/110 Clang and 109/109 optimized GCC
sanitizer groups. Android/JVM checks, debug/release builds, both lints, fixture
isolation, instrumentation-result controls and both-ABI 16 KiB alignment pass.
Next: continue authenticated change-state lifetime review and transparent
signing integration; physical custody and complete send remain open.

## Review completion-time publication — 2026-09-17

Resumed clean on backed-up `5fa772ddddc8b343b3064063cbbe511fc5ca12c3` and
fetched origin/main without integration. The existing signed-wire assembler
explicitly used one supplied entry timestamp and could not observe completion
freshness. A new internal C composition samples a trusted local monotonic clock
before admission and after all signature verification/serialization, then
rechecks the same live review before publishing privately staged bytes. Clock
failure, exact-deadline expiry and rollback publish nothing. Existing canonical
bytes, branch/context semantics and signature predicates are unchanged. This is
not wallet signing, custody/consent, authenticated chain state or send authority.

Deterministic tests pass both networks, 1..8 inputs, 1/16 outputs, exact signed
bytes, capacity/canary checks and entry/completion clock failures. A source-copy
fixture covers dirty backend refusal, malformed lengths and complete staging
retirement. Three isolated mutations of the second sample, final liveness check
or wipe fail. Production complexity remains <=10 and fixture complexity <=15;
the first combined function was split instead of raising the limit.

Unrelated change-state edits appeared during validation and were preserved.
Final gates used a source copy of the checkpoint plus only the six owned native
files, with a SHA256 manifest. That isolated snapshot passes 114 Clang sanitizer/
oracle groups and 109 optimized GCC sanitizer groups. Android JVM checks,
debug/test/minified-release builds, lints, fixture separation, instrumentation
result checks and 16 KiB alignment pass there. The x86_64 completion fixture
passes API30/35/36; both ABI fixture binaries are byte-identical when linked
from the isolated release archives. ARM64 execution and physical custody remain
unproven. The initial differential fuzz run completed 14,946 inputs without a
finding; the final isolated snapshot completed another 5,000. Its normal unsigned
release remains byte-identical to the checkpoint artifact, SHA256
`7c99584b556460ef04eeb727d0c6c67ee2eee60cbae725b9883da64bfe13ca68`.

Evidence: `.cache/android-wallet/mission-20260917/review-completion/` under the
wallet directory. The explicit native hazard review is in
[`C_SAFETY_REVIEW.md`](C_SAFETY_REVIEW.md), with the API boundary in
[`TRANSACTIONS.md`](TRANSACTIONS.md). No JNI or UI entry point was added. Next:
compose actual wallet signing only with authenticated custody, exact live review
ownership and qualified chain/consent inputs; preserve separate delayed-delivery
and broadcast checks. Physical-device acceptance and shielded send remain open.

## Instrumentation evidence completeness — 2026-09-17

The attended fixture checkpoint was committed and backed up as
`2b95c780df464b902e2c698ee83ba698830d81e1`, with exact remote SHA verification.
Its validation exposed a repeatable evidence bug: AndroidJUnitRunner emits
`OK (8 tests)` even when one test returns assumption-skip code `-4`. Counting
the summary alone accepted this incomplete run. The new bounded captured-log
checker rejects skips and failures, requires matching start/completion identities
and sequence for the explicit expected count, and checks the result stream,
summary and final runner status. Duplicate, malformed and partial evidence
refuses. Logs are bounded to 16 MiB and expected counts to 10000; the checker
does not authenticate devices, artifacts or test selection.

Three positive local fixtures cover plural/singular summaries, telemetry status
packets and CRLF. Thirty-seven negative cases cover skips, failures, unknown
statuses, identity/count/order mismatch, missing or duplicate events, malformed
fields, truncated/oversized/absent logs and invalid expected counts. All pass.
The registered `:android-app:checkInstrumentationResults` Gradle task passes and
is included in `check`. Debug/release lint, APK fixture isolation and both-ABI
16 KiB alignment pass. Shell syntax checks pass; shellcheck remains unavailable.
No C/JNI, product source, dependency or provider policy changes.

The saved eight-test API30/35/36 logs now refuse for their skipped per-use test.
Fresh explicitly selected six-boundary-plus-one-policy runs execute and pass all
seven tests on each API, with no skips, under the new checker. This establishes
those exact emulator observations only; the missing per-use/hardware proof is
unchanged. Evidence is under the wallet directory at
`.cache/android-wallet/mission-20260917/instrumentation-results/`. Use the checker
for future captured instrumentation acceptance, including the attended hardware
fixture. Next: continue custody/lifecycle review while actual physical-device
qualification and complete transparent/shielded send remain open.

## Attended public-vector custody fixture — 2026-09-17

Resumed the existing test-only slice on backed-up
`8f2f7f1a3dc70c714969eb75afac633bc4a710f9`; fetched origin/main without integration.
The qualification APK now has a separately guarded attended restore fixture.
Exact consent, package/test/debug flags, process/application/explicit UID and
known-emulator refusal precede any private-path or Keystore inspection. Admission
then requires a real parent directory, absent wallet path and absent fixed key
alias. File inspection errors, empty scaffolds and symlinks refuse. The fixture
never removes wallet/key state, injects device credentials, captures screens,
overrides provider policy or touches the normal wallet namespace.

The owner must authenticate a published unfunded BIP39 restore and two fresh
unlocks, with a 75-second bound per awaited stage. Assertions require the exact
testnet address, secure-window flag, unchanged committed record, no historical
change reset, accepted provider custody metadata and an authentication-specific
refusal from another unauthenticated operation. The public address was checked
with the existing independent OpenSSL oracle: 96 derivation comparisons and 48
recovered change bindings pass under sanitizers. Native source is unchanged.

All six synthetic admission regressions and the unauthenticated-key policy
refusal execute and pass on API30/35/36. The per-use provider test skips on all
three because no test screen lock is configured. An initial `OK (8 tests)`
summary check missed AndroidJUnitRunner's assumption code `-4`; stricter status
inspection corrected the earlier eight-pass commentary. It is seven passed,
one skipped, with no positive authentication evidence. On API35, the attended
fixture refuses the emulator, missing UID, malformed consent and normal package;
an omitted consent skips. The qualification wallet path remains absent afterward.
Only this invocation's qualification packages were removed. An older emulator's
unavailable prelaunch sandbox is retained as infrastructure evidence; the healthy
API35 instance supplies the actual admission-refusal observations.

A separate minimal APK contains only copied admission code, synthetic tests and
disposable cache files: its six-test control passes, and all nine isolated guard
mutations fail their intended missing-refusal assertion. These cover consent,
package, flags, UID, metadata, known emulators, parent directory, existing wallet
path and key alias. Mutation APKs were removed. Initial isolated harness Java/
Kotlin target mismatch was corrected to Java17; no product workaround was needed.

Normal JVM checks, debug/test/minified-release builds, both lints, fixture
isolation and 16 KiB alignment pass. Qualification identity checks and lint pass.
The normal unsigned release remains byte-identical, SHA256
`7c99584b556460ef04eeb727d0c6c67ee2eee60cbae725b9883da64bfe13ca68`.
This is instrumentation/documentation only; no C/JNI or product custody change.
Evidence: `.cache/android-wallet/mission-20260917/attended-custody/` under the
wallet directory. Commands and state-retention rules are in
[`DEVICE_QUALIFICATION.md`](DEVICE_QUALIFICATION.md).

No physical device is attached, so positive hardware execution remains open.
Creation/written backup, cancellation, invalidation, process-death/recovery and
minified hardware acceptance also remain open. Next: make instrumentation result
checking reject skipped/partial runs by default, then continue custody/lifecycle
readiness while preserving the physical-device acceptance requirement. TLS
quarantine and the historical unrelated root-lint limitation are unchanged.

## Physical-device qualification build boundary — 2026-09-17

Resumed clean from backed-up `3a9a278063b1cd255b84bad2c35cbccd20f7c5f3` and
fetched origin/main without integration. All four attached devices are emulators;
no physical custody claim can be made. The next hardware-fixture dependency is
now implemented: `-PwalletQualification=public-custody` builds a visibly labeled,
separate `org.zclassic.wallet.dev.qualification` debug package and matching
instrumentation package under an isolated cache output directory. Both are
explicitly test-only. Other property values and wallet release builds in this
mode fail. Default builds retain their original identities and output paths.

The new actual-APK identity gate and twenty negative metadata fixtures pass.
Its initial refusal exposed that AGP had not marked the instrumentation APK as
test-only; an explicit AndroidTest manifest now follows the selected profile.
Normal APK identity checking was also missing: the new regression first accepted
a wrong debug package against the inherited checker. The fixed checker refuses
all thirteen package/runner/target/profile mutations while retaining existing host,
process and asset checks. The release-host mutation now retains valid base
metadata so it still observes its intended host-exclusion refusal.

On API35, Android rejects qualification installation without test opt-in; with
explicit test installation, its UID differs from the normal app. All 28 public
unlock/seal/session-close fixtures pass in the separate namespace (0.783 seconds).
The unchanged emulator-only WalletFlow fixture refuses its package at admission,
before owning files or a Keystore alias. Both invocation-created qualification
packages are removed afterward. No physical device, real seed, funds or operator
wallet was involved. The hardware acceptance fixture remains to be implemented;
the build mode does not bypass authentication or custody policy.

Normal Android/JVM checks, both ABI debug/release builds, lints, fixture isolation
and 16 KiB alignment pass. Qualification lint, metadata mutations and alignment
also pass. Native source/provider/JNI policy is unchanged. The two profiles'
packaged native libraries differ only in the twenty GNU build-ID bytes on each
ABI; `.text`, `.rodata` and `.data` match. This is not whole-library byte identity.
The normal unsigned release remains byte-identical to the pre-change artifact:
SHA256 `7c99584b556460ef04eeb727d0c6c67ee2eee60cbae725b9883da64bfe13ca68`.
Shell syntax checks pass; shellcheck is unavailable. Two validation-only result
matchers initially treated literal punctuation as regex syntax; literal matching
confirmed the intended release/install/runtime refusals without rerunning work.

Build, installation, UID, mutation, native-comparison and cleanup evidence is
under `apps/zcl-wallet/.cache/android-wallet/mission-20260917/qualification-profile/`.
The supported mode and remaining attended hardware-fixture requirements are in
[`DEVICE_QUALIFICATION.md`](DEVICE_QUALIFICATION.md). Next: implement that guarded
public-vector fixture without changing the existing emulator fixture, then
obtain actual per-use hardware evidence on an explicitly selected fresh profile.
TLS remains quarantined. Physical custody and complete transparent/shielded send
remain unproven; historical global-lint limitations remain unchanged.

## Setup-expiry message accuracy — 2026-09-17

The preceding checkpoint was backed up and directly verified as
`bcfc19d323abb95519c5c6d217e8236f2021303f`. The setup-expiry message now directs
the user to check the saved wallet and keep the written backup. It no longer
claims immediate complete erasure: an active provider retains its input until
worker cleanup, as the existing creation/restoration closure fixtures observe.
Completed storage may also need a fresh unlock instead of a new setup.

Debug/release builds, both Android lints, fixture isolation and 16 KiB alignment
pass; both public-GCM closure fixtures pass on API35 in 0.057 seconds. This is
resource text only, with no native, policy, storage or ownership changes. Evidence
is under `apps/zcl-wallet/.cache/android-wallet/mission-20260917/setup-expiry-copy/`.
Next: continue custody and physical-device qualification readiness; retain the
explicit emulator-only fixture boundary until a separate safe hardware fixture
is reviewed. Positive hardware custody and complete send remain unproven.

## Setup and restore delivery lifetime — 2026-09-17

Resumed clean from backed-up `451c64e20bfa68fc3e24372c56d2386fb6e11342`;
origin/main was fetched without integration. The inherited creation and restore
workers checked setup before persistence, but their queued success callbacks
could reach `received()` after expiry and cancel the overdue setup timer.
The new deterministic regression fails both paths against that implementation;
the final-live-millisecond control already succeeds.

Both paths now share a small delivery helper that checks their original
C-backed setup window immediately before reporting the saved address. It
captures only the public window/address and existing callbacks, after entropy
cleanup, without retaining the old Setup object. Expiry or backward time reports
the existing operation failure. Completed wallet bytes remain unchanged; fresh
creation retains paired change state, and restore still cannot initialize it.
This does not interrupt providers or roll back a completed durability protocol.

All 28 selected unlock/seal/session-close tests pass on API30 (8.401 seconds),
API35 (0.797 seconds) and API36 (16.136 seconds). New cases cover both actions,
rollback, exact expiry, extreme elapsed time and the final live millisecond;
they also observe plaintext retirement and preserved stored records. Three
isolated source mutations bypass creation routing, restore routing or the shared
positive-remaining check. They fail one, one and two targeted regressions,
respectively; the unchanged control passes all three. Exact mutation diffs were
inspected before execution. The separate, initially absent
`org.zclassic.wallet.dev.setupmutation20260917` package and its test package are
removed afterward; the normal wallet installation and release remain unchanged.

Android/JVM checks, both ABI debug/release builds, minification, lints, fixture
isolation and 16 KiB alignment pass. No C, JNI, provider or cryptographic bytes
changed, so the preceding full native safety and focused custody/recovery
evidence retain their exact scope. Review confirms the helper reads the existing
window only at delivery, closed-session callbacks remain inert, no secret is
captured, and failure cannot delete or rewrite completed wallet/change state.

Staged source tree `a167447982bc337a15a532fcc6a3ac346e6e8e09` rebuilds the unsigned
release byte-identically in a fresh directory on the same host/toolchain:
631,223 bytes, SHA256
`b744cd2a08940060031d05360a833c2ca8d36983e345e4b65949de72433c95b1`.
Architecture/doc counts pass. Evidence lives under
`apps/zcl-wallet/.cache/android-wallet/mission-20260917/setup-delivery/`.

Next: continue custody/backup failure review and authenticated signing/change
acceptance from the existing primitives. Physical hardware custody, real camera
qualification, authenticated chain context and complete transparent/shielded
send remain unproven. These public software-GCM emulator fixtures grant none of
those claims. TLS stays quarantined and historical global-lint limits remain.

## Post-authentication unlock lifetime — 2026-09-17

Resumed clean from backed-up `c42e70e4be99b303e93e7ddd47c01db70dc5120d`;
origin/main was fetched without integration. Inspection found that the prompt
had a 90-second continuation bound, while the subsequent unlock had no elapsed
guard at worker entry, pending promotion or delayed public UI delivery.

`UnlockWindow` now captures one origin at submission and delegates its checks
to the existing C authentication-window predicate. The worker checks before
decrypting and after plaintext erasure before admitting promotion; UI delivery
checks again. Expiry/backward clocks report the existing operation failure.
Already completed durability is preserved and a late delivery requires a fresh
unlock. This does not preempt provider calls or promise a filesystem deadline.
Prompt authentication, closed-owner refusal, exact GCM/address/record checks
and setup's separate ten-minute policy remain unchanged.

With the new clock-bearing invocation but inherited unenforced unlock body,
three of four boundary cases fail: expired entry still calls the provider,
post-decrypt expiry still promotes, and expired queued delivery still succeeds.
All four pass after enforcement, including the final live millisecond. The
full 25-case unlock/seal/session-close matrix passes on API30 (8.443 seconds),
API35 (0.577 seconds) and API36 (14.140 seconds), using only public software-GCM
fixtures in isolated cache directories. No Keystore alias or real seed is used.

Three independent source-copy mutations each remove one guard and fail exactly
its intended regression; the unchanged control passes 4/4. They run under the
separate, initially absent `org.zclassic.wallet.dev.unlockmutation20260917`
application ID; both invocation-owned packages are removed afterward. An
initial broad edit selected a creation check instead of unlock and correctly
survived these unlock tests. The edit was scoped to the unlock function, each
exact diff was inspected, and the intended mutations then failed. No mutation
changes the checkout, normal wallet installation or release artifact.

Both new JVM clock tests and the full Android/JVM checks pass, along with both
ABI builds, minified release, lints, fixture isolation and 16 KiB alignment.
Ten focused ASan/UBSan/LSan native custody/JNI/record/confirmation/recovery cases
pass (11.46 and 0.30 seconds across two runs). No C, JNI ABI, provider or
cryptographic semantics changed; the preceding full 108/107 Clang/GCC safety
checkpoint remains applicable. Review confirms new state contains only a public
clock origin/callable, all decrypted entropy clears before persistence or UI,
and failure/cancellation retains existing ownership and cleanup paths.

Fresh staged source tree `023b8b80cce0d996d14d7e1852de30e17d08939e` reproduces
the unsigned release byte-for-byte in another directory on the same host and
toolchain: 631,223 bytes, SHA256
`1ede05673196d4778a19c8a73ea5ffc0292b1e300fafa37fa3d0c4b9389afdd6`.
Architecture/doc counts pass. Evidence is under
`apps/zcl-wallet/.cache/android-wallet/mission-20260917/unlock-window/`.

Next: review setup/restore completion and UI delivery against their existing
immutable window, then continue authenticated signing/change-state acceptance.
Physical hardware custody, real camera qualification and authenticated chain
context remain open; emulator/public-GCM evidence proves none of these. TLS
stays quarantined, and historical global-lint limitations remain unchanged.

## Transaction codec continuation — 2026-09-17

The assessment checkpoint was backed up and directly verified as
`1e5e43b542b2f76b4dfc9546906dd6aa8df1f003` on the same-named development branch.
Its remaining four API36 render-refusal tests subsequently pass separately in
3.243 seconds; the earlier combined/host timeouts remain preserved below.

The parser and serializer now retire reversed input hashes immediately after
copying them, plus the staged parsed transaction/full wire byte array on every
initialized exit. Exact serialization and all existing admission, profile and
caller-output guarantees remain unchanged. This retires public transaction
metadata; it grants no custody, consensus or spending authority.

The new registered fixture fails against inherited cleanup. It passes all three
pinned canonical transactions, every truncation, exact serialization, short
capacity and injected validation/size-disagreement failures. Each of four
isolated cleanup-removal mutations fails; the unchanged source-copy control
passes. An initial mutation-only archive link-order error was corrected with
an explicit linker group, without changing product code. Explicit C hazard
review is recorded in C_SAFETY_REVIEW.md.

Full non-TLS safety passes 108/108 Clang cases (87.52 seconds) and 107/107
optimized GCC cases (124.76 seconds), both analyzers and unchanged production/
test complexity caps 10/15. The new fixture separately passes both analyzers
and strict ARM64/x86-64 NDK compilation. Seeded ASan/UBSan transaction and
assessment campaigns complete 577,142 and 1,558,312 executions, each in 61
seconds with a five-second input deadline and 512 MiB RSS cap, without findings.
Android/JVM checks, both ABI debug/release builds, lints, fixture isolation and
16 KiB alignment pass. All thirteen selected API35 review/lifecycle/render
cases pass in 5.035 seconds; five public unsigned-review cases pass on API30
(1.348 seconds) and API36 (3.174 seconds). ARM64 remains compile-only here.

A fresh directory archived from staged source tree
`f4966b87aac7ade0e537f0e85c64fcef79969cb6` rebuilds the release APK byte-identically
on the same host/toolchain: 631,223 bytes, SHA256
`9e9134795ec86f4fb51d687826c8ed884016ae73f95e97d4a7ce4f57489fb6e5`.
This does not establish independent-host reproduction or physical-device custody.
Evidence lives under
`apps/zcl-wallet/.cache/android-wallet/mission-20260917/codec-retirement/`.

Next: return to per-use custody and deterministic recovery refusal coverage,
then signing/change-state completion. Inspect existing contracts before treating
primitive-level limits as defects. Hardware-authenticated custody and fresh
authenticated transparent-send composition remain unproven; TLS stays
quarantined. No production state or funds were accessed, and historical
global-lint limitations remain unchanged.

## Assessment scratch continuation — 2026-09-17

The preceding checkpoint was backed up and directly verified as
`c30ec3193ffcc3a109074ceefab385d4c4584557` on the same-named development branch.
Assessment now retires each local previous output, parsed destination and
staged report on every initialized exit. Checked totals/fees, exact transaction
identity and complete-only caller publication remain unchanged. This is public
metadata lifetime hardening, not new chain freshness or spending authority.

The new registered regression fails against inherited output retirement and
passes with the fix through every dirty provider stage, overspending, aggregate
input overflow and fee-ceiling refusal. Removing each of the three cleanup
calls in isolated source copies independently fails; the unchanged control
passes. Explicit C hazard review is recorded in C_SAFETY_REVIEW.md.

Full non-TLS safety passes 107/107 Clang cases (88.09 seconds) and 106/106
optimized GCC cases (123.32 seconds), both analyzers and unchanged production/
test complexity caps 10/15. The new fixture separately passes both analyzers
and strict ARM64/x86-64 NDK compilation. A seeded ASan/UBSan assessment campaign
completes 1,553,815 executions in 61 seconds with a five-second input deadline
and 512 MiB RSS cap, without findings. Android/JVM checks, both ABI debug/release
builds, lints, fixture isolation and 16 KiB alignment pass. All thirteen selected
review/lifecycle/render-refusal tests pass on API30 (70.948 seconds) and API35
(5.428 seconds). The combined API36 run exceeded its 90-second bound after
seven completed tests. Individual-class retries report five unsigned-review
tests (2.988 seconds) and four lifecycle tests (107.21 seconds) passing, but the
120-second host deadline still expired before the remaining render class ran.
API36 therefore has partial evidence only; logs preserve the timeout instead
of treating it as a complete pass. API30/35 provide the complete selected matrix.

A fresh directory archived from staged source tree
`1289695cd4dd8b1e7e09ab43c9f13bfb04363e81` rebuilds the release APK byte-identically
on the same host/toolchain: 631,143 bytes, SHA256
`c6346e61c618b4ff6f46601f2049794f2b2ac07d9ec4e5509a925d7bfebc32c6`.
This is neither independent-host nor physical-device evidence. Architecture/
doc counts pass. Logs and isolated artifacts are under
`apps/zcl-wallet/.cache/android-wallet/mission-20260917/assessment-retirement/`.

Next: parser/serializer staging lifetime and canonical round-trip/refusal
regressions. Hardware-authenticated custody and authenticated transparent-send
composition remain unproven; TLS remains quarantined. No production state or
funds were accessed. Historical global-lint limitations remain unchanged.

## Transaction ID and prevout continuation — 2026-09-17

Resumed from clean, backed-up `432872cf6`; origin/main was fetched without
merging. Transaction ID and previous-output helpers now retire canonical wire,
both SHA256d digests, the parsed previous transaction and compared ID on every
initialized exit. Exact hash bytes/order, output-index selection and failure
publication semantics remain unchanged. This proves no chain inclusion,
unspentness, custody or spending authorization.

The new registered fixture fails against inherited ID cleanup, then passes
with the fix. It covers all three pinned transaction projections and every
output, dirty parser/serializer/both SHA failures, wrong hash and invalid index,
with complete caller-output preservation. Five isolated cleanup-removal mutants
fail their retirement assertions; the unchanged source-copy control passes.
Explicit C hazard review is recorded in C_SAFETY_REVIEW.md.

Full non-TLS safety passes 106/106 Clang wallet CTest cases (86.89 seconds) and
105/105 optimized GCC cases (123.06 seconds), both analyzers and unchanged
production/test complexity caps 10/15. The new fixture also passes both
analyzers and strict ARM64/x86-64 NDK compilation. Android/JVM checks, both ABI
debug/release builds, lints, fixture isolation and 16 KiB alignment pass.
All thirteen selected API35 review/lifecycle/render-refusal cases pass in
5.462 seconds. Architecture/doc counts pass.

Bounded ASan/UBSan campaigns exercise transaction and assessment inputs; the
initial unseeded runs complete 649,566 and 5,724,528 executions. Separate final
campaigns start from canonical public previous transactions and a draft, so
accepted hash/prevout/accounting paths participate immediately. Every campaign
has a 61-second observed duration, five-second input deadline and 512 MiB RSS
cap, without findings. Logs and corpus copies are under
`apps/zcl-wallet/.cache/android-wallet/mission-20260917/transaction-retirement/`.

Next: assessment-local previous outputs, parsed destinations and staged report
retirement, preserving checked totals/fees and complete-only publication.
The broader acceptance review still leaves hardware-authenticated custody and
authenticated transparent-send composition unproven. TLS remains quarantined;
physical-device and historical global-lint limitations remain unchanged.

## Wallet header and recovery continuation — 2026-09-17

The BIP32 checkpoint was backed up and directly verified as
`ce8002b3039e30f0c1d6ed9c6d1dcb9a2c5b27ec` on the same-named development branch.
Wallet-header creation, parsing and recovered-address validation now retire
their initialized header/address/identity scratch on success and every refusal.
Exact header/profile checks, re-derived address binding and caller-output
preservation remain unchanged. Parsing still does not authenticate a record;
platform GCM authentication remains a prerequisite to recovery acceptance.

A new registered source-copy fixture fails on inherited creation's missing
clear and passes with the fix. It covers dirty genesis/address/derivation
providers, impossible returned lengths, unsupported P2SH, changed genesis,
profile/entropy mismatches, output canaries and early capacity/NULL refusal.
Six separate isolated mutations remove each category of cleanup; every mutant
fails its live-retirement assertion, while the unchanged source-copy control
passes. No mutation enters the checkout or Android artifact. Existing record
mutation matrices remain active; the explicit hazard review is recorded.

Full non-TLS safety passes 105/105 Clang wallet CTest cases (86.44 seconds) and
104/104 optimized GCC cases (122.20 seconds), both analyzers and unchanged
production/test complexity caps 10/15. The new fixture separately passes both
analyzers and strict ARM64/x86-64 NDK compilation. A bounded ASan/UBSan record
fuzz campaign completes 1,404,949 executions in 61 seconds, five-second input
deadline and 512 MiB RSS cap, without findings. Android/JVM checks, both ABI
debug/release builds, lints, fixture isolation and 16 KiB alignment pass.
All nineteen selected API35 record/recovery/unlock cases pass in 0.646 seconds.
Architecture/doc counts pass. Evidence is under
`apps/zcl-wallet/.cache/android-wallet/mission-20260917/header-retirement/`.

Next inspected gap: `transaction_id.c` and `transaction_prevout.c` retain wire,
digest and parsed previous-transaction scratch across success/early failure.
Qualify cleanup with dirty-provider/live-clear regressions while preserving
exact transaction IDs, hash-bound output selection and output atomicity.
TLS quarantine, hardware-custody/physical-device gaps and historical global-lint
limitations remain unchanged. No production state or funds were accessed.

## BIP32 private input continuation — 2026-09-17

The preceding sync checkpoint is backed up and directly verified as
`a0fe489d083eb6780b93835a79aba7f415cdf070` on the same-named wallet-backup branch.
BIP32 child derivation now clears its 37-byte input immediately after HMAC,
before scalar tweaking; hardened input includes a redundant private-key copy.
Preparation failure still clears it. No derivation bytes, index policy,
invalid-child behavior, provider calls or caller-output semantics change.

The inherited implementation fails the new real-tweak-entry retirement
assertion. Twenty normal/hardened boundary-index and provider-fault combinations
now observe full input/digest retirement, unchanged parent, exact outputs and
existing refusal statuses, including dirty HMAC failure and order/negative/zero
tweaks. A small test-helper extraction restored complexity 15 from the initial
16 without removing assertions. Explicit hazard review is in C_SAFETY_REVIEW.md.

Full non-TLS safety passes 104/104 Clang wallet CTest cases (86.17 seconds) and
103/103 optimized GCC cases (123.17 seconds), both analyzers and unchanged
production/test complexity caps 10/15. Changed fixtures pass both analyzers and
strict ARM64/x86-64 NDK compilation. Published BIP32 vectors, independent OpenSSL
differential checks and receive-address oracle pass (three groups, 9.32 seconds).
The 860-case public differential replay seeds an ASan/UBSan fuzz campaign:
4,827 executions in 61 seconds, five-second input deadline, 512 MiB RSS cap,
without findings. Android/JVM checks, both ABI debug/release builds, lints,
fixture isolation, 16 KiB alignment and architecture/doc counts pass.

API35 passes twelve key/record/recovery cases in 0.370 seconds. API30 and API36
each pass all three key-adapter cases in 0.900/1.066 seconds. These are x86-64
emulator observations; hardware custody and ARM64 device execution remain open.
Source-only tree `9f4424af0c1780238923757afae89c429217498f` reproduces the complete
unsigned release APK in a different checkout path on this host/toolchain:
630,663 bytes, SHA256
`e5593e6345195d313db1d5b39598955abab8fbd7545dbaf6239b5cd8cd07cc45`.
Final app source differs from that tree only in progress/safety documentation.
Evidence: `apps/zcl-wallet/.cache/android-wallet/mission-20260917/bip32-input-retirement/`.

Next inspected gap: wallet-header parsing, creation and recovered-address
validation retain address/identity scratch on early returns. Qualify cleanup
without relaxing exact header binding or output preservation. TLS remains
quarantined; no global-lint, physical-camera or production acceptance is claimed.

## Native sync parser continuation — 2026-09-17

Resumed clean at `e8450172c` on the existing Android development branch; fetched
current origin/main without merging or changing upstream permissions. The next
recorded gap in `sync.c` is closed: validation-only parsed addresses and both
tip-comparison temporaries now retire after their last use, including dirty
provider refusal. Abort deliberately retains address/network routing metadata;
phase, request-ID advancement and complete-only report publication are unchanged.

The strengthened existing sync fixture fails on inherited code at its first
missing-clear assertion, then passes with the fix. It checks full live erasure,
dirty parser refusal, dirty tip refusal at both phases, exact routing retention,
unchanged request IDs and unavailable reports, alongside the existing mainnet/
testnet, changed-tip, malformed-input, boundary-ID and output-canary cases.
The explicit C hazard review is in `C_SAFETY_REVIEW.md`.

Full non-TLS safety passes all 104 Clang wallet CTest cases (86.28 seconds) and
103 optimized GCC cases (122.65 seconds), with ASan/UBSan/leak detection, both
analyzers and unchanged production/test complexity caps 10/15. The changed
fixture also passes both analyzers and strict ARM64/x86-64 NDK compilation.
A bounded ASan/UBSan sync fuzz run completes 49,896 executions in 61 seconds,
with five-second input deadlines and a 512 MiB RSS cap, without findings.

Android/JVM checks, both ABI debug/release builds, lints, fixture isolation,
16 KiB alignment and architecture/doc counts pass. All sixteen selected sync,
balance-lifecycle and storage/recovery cases pass on accelerated API35 in
5.146 seconds. The old API35 profile exceeded the 90-second host deadline with
no result output; that failure is preserved and no acceptance is claimed for
it. No emulator reset or test assertion weakening was used. Evidence is under
`apps/zcl-wallet/.cache/android-wallet/mission-20260917/sync-scratch-retirement/`.

Next: qualify BIP32 child-input retirement immediately after HMAC consumption,
before scalar tweaking. Hardened child scratch contains a private-key copy;
preserve exact derivation, invalid-child status and caller-output atomicity.
TLS remains quarantined; hardware custody, physical-camera acceptance and
historical global-lint limitations remain open. No production state was touched.

## Native sync watch continuation — 2026-09-17

The independently finalized JNI storage slice was pushed and remotely verified
as `480f8d65ed01cc3305abf3f098593ef66118e96a`. Continued into native watch scratch:
parsed-address validation, staged report publication and snapshot temporaries
now clear after their final use. Successful retained/caller values remain intact;
failed report extraction preserves the previous report and fails the attempt.

The existing watch fixture now observes full erasure while each object is live.
The inherited implementation fails its first missing-clear assertion. Dirty
parser refusal checks that initialization leaves an empty watch; dirty report
refusal checks exact prior-report preservation, stale display state and late-token
refusal. Existing both-network, retry, phase, deadline/backward-clock and output
canary assertions remain active. Explicit hazard and frame review is recorded
in `C_SAFETY_REVIEW.md`.

Bounded ASan/UBSan campaigns complete 51,361 native watch and 13,370 JNI sync
executions, 61 seconds each, without findings (five-second input deadlines,
512 MiB RSS caps). Android/JVM tests, both ABI debug/release builds, lints,
fixture isolation and 16 KiB alignment pass. All 24 selected API35 sync,
lifecycle and storage/recovery cases pass in 8.163 seconds. Architecture/doc
counts and production/test complexity caps 10/15 pass. Evidence is under
`apps/zcl-wallet/.cache/android-wallet/mission-20260917/sync-watch-retirement/`.
TLS remains quarantined, production state is untouched, and no global lint pass
or hardware-custody qualification is claimed.

Full non-TLS safety passes all 104 Clang wallet CTest cases (86.02 seconds) and
103 optimized GCC cases (122.18 seconds), including both static analyzers and
strict compiler gates.

Continuation inspection: `sync.c` still has parsed-address and tip-comparison
scratch with early returns. Qualify those with dirty-provider/live-clear tests
while preserving phase/request-ID advancement and complete-report publication.
Abort intentionally retains address/network routing metadata; do not clear the
whole candidate as a shortcut. The broader wallet mission remains open.

## Inherited JNI storage slice finalized — 2026-09-17

JNI sync opening was pushed and remotely verified as
`07aa78319a7bd43279981848ab89763d353c989f`. The inherited storage changes were
kept separate throughout the six new hardening slices, then independently
reviewed and qualified for their own checkpoint. Their source and test changes
are preserved intact: JNI path/record/read-packet copies retire on all exits,
and fresh creation retains entropy-first cleanup. Input/status/record authority,
no-overwrite and exact-pending promotion semantics remain unchanged.

A fresh isolated build of the preceding JNI storage source with the current
fixture aborts at the expected missing full-copy retirement assertion. The two
focused JNI storage tests pass on both Clang and optimized GCC. A bounded
ASan/UBSan fresh-storage fuzz run completes 21,498 executions in 61 seconds,
with a five-second input deadline and 512 MiB RSS cap, without findings. Its
only valid path is the invocation-owned fixture directory; malformed path
lengths cannot resolve a different ambient directory.

The final source tree was included in the JNI opening validation: all 104 Clang
and 103 optimized GCC wallet CTest cases pass, both analyzers and complexity
caps 10/15 pass, and Android/JVM tests, both ABI debug/release builds, lints,
fixture isolation, 16 KiB alignment and architecture/doc counts pass. All 24
selected API35 sync/lifecycle/storage/recovery cases pass in 8.028 seconds.
Storage-specific evidence is under
`apps/zcl-wallet/.cache/android-wallet/mission-20260917/jni-storage-final/`;
shared full-gate evidence is under the JNI opening evidence directory.
The explicit hazard review remains in `C_SAFETY_REVIEW.md`. No global lint pass,
hardware custody, TLS or production-node acceptance is claimed. TLS remains
quarantined; no production data or credentials were accessed.

Continuation inspection: `sync_watch.c` still has unretired validation-only
parsed-address, publication-report and snapshot temporaries. Preserve the
existing clock/timeout, last-report, source and attempt-token contracts while
qualifying their retirement with live observers.

## JNI sync opening continuation — 2026-09-17

Native owner retirement was pushed and remotely verified as
`5f87a0aa3810264d7cea072af2913afe98abc141`. Continued immediately into JNI
owner input copies. Address/source arrays now retire on every return and before
registry unlock when a lock was acquired. Input/status precedence, pending
exceptions, pool capacity and monotonically issued owner IDs are unchanged.

The inherited implementation fails the new live-retirement assertion. Both
owner modes now cover NULL VM/inputs, pending exceptions, invalid chain,
oversized arrays, malformed address and dirty partial reads of either input.
Each refusal is followed by successful creation/close. Existing replacement,
request/snapshot cleanup and fuzz paths observe the same full-span clears.
Explicit hazard review is in `C_SAFETY_REVIEW.md`.

Full non-TLS safety passes 104/104 Clang wallet CTest cases (85.66 seconds) and
103/103 optimized GCC cases (122.21 seconds), both analyzers and complexity caps
10/15. Optimized JNI opening uses a 192-byte bounded frame. The ASan/UBSan JNI
sync fuzzer completes 13,898 executions in 61 seconds without findings, with a
five-second input deadline and 512 MiB RSS cap. Android/JVM tests, both ABI
debug/release builds, lints, fixture isolation and 16 KiB alignment pass. Combined
API35 sync/lifecycle plus storage/recovery instrumentation passes all 24 cases
in 8.028 seconds. Architecture/doc counts pass. Full runs include the inherited
storage slice, which remains independent and is being finalized separately.
Evidence is under
`apps/zcl-wallet/.cache/android-wallet/mission-20260917/jni-sync-open-retirement/`.
TLS and production boundaries remain unchanged; no global lint pass is claimed.
Next: publish the separately reviewed and validated inherited JNI storage slice,
then inspect native watch initialization/report/snapshot temporary lifetime.

## Native sync owner continuation — 2026-09-17

JNI request retirement was pushed and remotely verified as
`aeb15eba33727a1728fe7c72795a0c46c14e7351`. Continued into the owner registry:
opening retires its staged watch on success/refusal, and close/clear-all now use
the secure-zero primitive for complete retained slots. The issued-ID counter,
capacity, stale-owner refusal and failure-atomic pool/ID publication are preserved.

The new registered fixture observes real full-span erasure while each object
is live. Both initialization modes cover dirty provider refusal, malformed
address/network/source length, slot reuse, full capacity and ID saturation.
The inherited implementation fails the first missing staging-clear assertion.
Existing owner and JNI suites also pass. Explicit hazard review and bounded
frame measurements are in `C_SAFETY_REVIEW.md`.

Full non-TLS safety passes 104/104 Clang wallet CTest cases (85.86 seconds) and
103/103 optimized GCC cases (122.20 seconds), both analyzers and complexity caps
10/15. Bounded ASan/UBSan campaigns complete 29,610 owner-history and 13,597 JNI
sync executions, 61 seconds each, without findings (five-second input deadlines,
512 MiB RSS caps). Android/JVM tests, both ABI debug/release builds, lints,
fixture isolation and 16 KiB alignment pass. Thirteen API35 sync/history/
lifecycle/render-failure cases pass in 8.078 seconds; architecture/doc counts
pass. These full runs include the preserved inherited storage work, independent
of this slice. Evidence is under
`apps/zcl-wallet/.cache/android-wallet/mission-20260917/sync-owner-retirement/`.
TLS and production boundaries remain unchanged; no global lint pass is claimed.
Next: retire JNI owner-opening address/source copies across partial reads and
early returns, including pending exceptions and malformed admission.

## JNI sync request continuation — 2026-09-17

JNI sync snapshot retirement was pushed and remotely verified as
`233bf411b17481e74873f7aa411b2a789bd18d25`. Continued immediately into request
publication: the staged watch and full packet now clear after transfer/refusal
and before unlock. Success still publishes only after Java accepts the packet;
failed publication retains the existing current-token/stale-token behavior.

The expanded JNI fixture checks real erasure while both objects remain live
on every entered request helper. Its inherited-source build fails the first
missing-clear assertion. All four New/Set failures now cover active requests
and stale tokens with extreme clocks; stale failure cannot expire or fail the
current attempt. VM transfer exceptions, NULL/pending entry and successful history
progression remain covered. The fuzzer predicts both returned object and pending
exception for all injected allocation modes, alongside the live-clear checks.

Full non-TLS safety passes 103/103 Clang wallet CTest cases (85.46 seconds) and
102/102 optimized GCC cases (121.46 seconds), both static analyzers and
production/test complexity caps 10/15. The optimized request frame remains
1840 bytes, below 4096. Bounded ASan/UBSan JNI sync fuzzing completes 14,433
executions in 61 seconds without findings (five-second input deadline, 512 MiB
RSS cap). Android/JVM tests, both ABI debug/release builds, lints, fixture
isolation and 16 KiB alignment pass. Thirteen selected API35 sync/history/
lifecycle/render-failure cases pass in 7.718 seconds; architecture/doc counts
pass. Broad runs include preserved inherited storage work, independently of this
slice. Evidence is under
`apps/zcl-wallet/.cache/android-wallet/mission-20260917/jni-sync-request-retirement/`.
TLS and production boundaries remain unchanged; no global lint pass is claimed.
Next: native sync-owner staged-watch and retained-slot retirement, preserving
pool identity and failure-atomic opening.

## JNI sync snapshot continuation — 2026-09-17

Native draft retirement was pushed and remotely verified as
`1f76ae41107998ba4aad8a34a2c2371943e44db5`. Continued into JNI balance/history
snapshots: native report copies now clear before registry unlock and Java
allocation; numeric output arrays clear at full capacity on success/refusal.
Clock/timeout effects, source identity, packet shapes, pending exceptions and
replacement ownership retain their existing semantics.

The existing source-copy fixture now checks live erasure with the real primitive,
including unused numeric capacity. It covers dirty provider refusals, invalid
age/balance/deadline/history fields, maximum history, stale owners, entry
refusals and all four New/Set failure classes. Failed publication is interleaved
with owner replacement for both response shapes, outside the native lock.
The inherited implementation fails its pre-allocation retirement assertion.
The JNI sync fuzzer includes the same observers and publication faults.

Full non-TLS safety passes all 103 Clang and 102 optimized GCC wallet CTest
cases, both static analyzers and production/test complexity caps 10/15. JNI sync
ASan/UBSan fuzzing completes 15,158 executions in 61 seconds with a five-second
input deadline and 512 MiB RSS cap, without findings. Android/JVM tests, both ABI
debug/release builds, lints, fixture isolation and 16 KiB alignment pass. All
thirteen selected API35 sync/history/lifecycle/render-failure cases pass in
8.235 seconds. Architecture/doc counts pass. Full suites include the preserved
inherited storage work; these changes are independent of it. Evidence is under
`apps/zcl-wallet/.cache/android-wallet/mission-20260917/jni-sync-snapshot-retirement/`.
TLS remains quarantined, no production data is touched, and the historical global
lint failures remain open. Next: retire JNI request watch/packet scratch while
preserving publication atomicity and stale-attempt refusal.

## Native draft continuation — 2026-09-17

The preceding JNI draft slice was pushed and remotely verified as
`70ee507bd23c64353c6f6c3195eb47783404630d`. Continued into native construction:
staged candidate, parsed funding, assessment and source-descriptor scratch now
clear on success and every initialized refusal. Exact input/output ordering,
fee policy, canonical txids and failure-atomic caller publication remain intact.

The new registered fixture uses real secure-zero observers on live objects and
injects dirty parser, txid and assessment failures. It verifies cleanup before
scratch reuse, before assessment and before outer candidate retirement, alongside
unchanged caller transaction and request bytes on refusal. The inherited source
fails before its second funding parse. Existing exact-wire and maximum-row tests
also pass. Optimized GCC frame reports remain below 4096 for each changed entry;
explicit hazard review is in `C_SAFETY_REVIEW.md`.

Full non-TLS safety passes 103/103 Clang wallet CTest cases (85.34 seconds) and
102/102 optimized GCC cases (121.41 seconds), both analyzers and complexity caps
10/15. Bounded ASan/UBSan campaigns complete 98,185 native draft and 20,661 JNI
draft executions, 61 seconds each with five-second input deadlines and 512 MiB
RSS caps, without findings. Android/JVM tests, both ABI debug/release builds,
lints, fixture isolation and 16 KiB alignment pass. The thirteen selected API35
review/lifecycle/render cases pass in 5.718 seconds. Architecture/doc counts pass.
These broad runs include the unchanged inherited JNI storage work; this slice
is independent of it. No global lint pass, hardware custody or TLS qualification
is claimed. Evidence is under
`apps/zcl-wallet/.cache/android-wallet/mission-20260917/draft-retirement/`.
Next inspected gap: JNI sync balance/history snapshot and numeric output scratch
are not retired. Preserve native clock/timeout and replacement-owner semantics.

## JNI draft continuation — 2026-09-17

This session found the requested JNI review cleanup already pushed and verified
at `de59fd23c71b7fdf0e46b55fb8e155bbf59a884e`. Inherited uncommitted JNI storage
source/tests/documentation are preserved separately and excluded from this slice.
The next inspected gap was JNI draft scratch: numeric parameters, destination
text, the constructed transaction and wire response survived their last use.
They now clear at full capacity on success and failure, preserving validation,
exception state, output format and existing heap-input retirement.

The source-copy fixture observes actual erasure while each object is live,
requires retirement before VM allocation, poisons transaction output before
provider calls, and covers partial reads and all four New/Set fault outcomes.
The inherited implementation fails the first missing-retirement assertion.
The same observer and expanded fault class run in the existing JNI draft fuzzer.
Short wire buffers retain their caller byte/length canaries. No consensus,
custody, signing, network or TLS behavior changes.

Android/JVM tests, ARM64/x86-64 debug/release builds, lints, fixture isolation
and 16 KiB alignment pass. All thirteen selected API35 review/lifecycle/render
failure cases pass in 5.616 seconds. Bounded ASan/UBSan JNI draft fuzzing completed
19,111 executions in 61 seconds (five-second input deadline, 512 MiB RSS cap)
without findings. Architecture/doc counts and production/test complexity caps
10/15 pass. Explicit hazard review is in `C_SAFETY_REVIEW.md`; evidence is under
`apps/zcl-wallet/.cache/android-wallet/mission-20260917/jni-draft-retirement/`.
The historical repository-wide lint failures remain documented; no global lint
pass is claimed. TLS stays quarantined and production data is untouched.

Full non-TLS safety passes all 102 Clang wallet CTest cases (84.52 seconds) and
101 optimized GCC cases (120.75 seconds), including both static analyzers and
strict compiler gates. These runs include the preserved inherited storage work;
the isolated draft changes do not depend on it. Next: native draft candidate,
parsed funding and assessment scratch retirement.

## Current continuation — 2026-09-17

Continued from the clean, remotely verified `63f3e2f43` checkpoint in
`/root/z23-android` on `agent/android-jni-secret-retirement-20260915`.
The development push target remains that same branch on `wallet-backup`.
Current `origin/main` was fetched; unrelated node changes remain outside this
wallet-only branch under its no-merge contract.

The underlying C record codec now retires staged ciphertext/metadata, extending
the preceding JNI cleanup. Packing validates before direct publication and
removes a redundant 140-byte ciphertext array. Parsing clears its entire staged
record on success and all refusals after initialization. Caller outputs remain
unchanged on failure. Source-only test hooks assert native retirement and track
full JNI input/result capacities through all existing VM faults, partial reads,
allocation failures, malformed inputs, NULL inputs and pending exceptions.
The strengthened native fixture fails against the inherited codec at its first
missing-cleanup assertion; the current native/JNI fixtures both pass.

Clang safety passes 99/99 wallet CTest cases in 82.03 seconds; optimized GCC
passes 98/98 in 119.05 seconds. Both static analyzers and production/test
complexity caps 10/15 pass. Separate bounded ASan/UBSan fuzz runs complete
175,703 JNI-record and 1,176,614 native-record executions, 61 seconds each,
with five-second input deadlines and 512 MiB RSS limits, without findings.
Android/JVM tests, ARM64/x86-64 builds, debug/release APKs and lints, fixture
isolation, 16 KiB alignment, architecture and documentation-count checks pass.
The API35 public in-memory record/GCM instrumentation test passes all ten
entropy/network combinations in 0.132 seconds on `emulator-5560`. The earlier
`emulator-5554` invocation timed out after 90 seconds without test output and
is not a pass. No wallet directory or Keystore alias is opened by this fixture.
Repository-wide lint also reports failures outside the changed wallet surface
(including Tor defaults, retired protocols, hard-link seeding and Windows
syntax). Its full local log is retained; no global lint pass is claimed.

Evidence is local under
`apps/zcl-wallet/.cache/android-wallet/mission-20260917/record-retirement/`.
The explicit hazard review is in `C_SAFETY_REVIEW.md`. This qualifies bounded
record-copy retirement, not hardware custody or erasure of every managed or
provider copy. TLS remains quarantined; positive hardware custody, physical
camera acceptance, authenticated sending and real sync remain unfinished.
This batch was pushed as `306dc9a2bfbd5d241c4964ebf5c159618b603fba`;
the remote advertised that exact SHA. The existing per-command development
backup hook exception was used; installed hooks remain unchanged.

Continued immediately into storage read/create/promotion scratch retirement.
Every initialized 140-byte read/comparison array now clears on success and
refusal; validation-only parsed records clear before filesystem work. The
promotion comparison is complete and its copy is retired before commit or
idempotent directory sync. Output publication, exact-record comparison, status
precedence and no-overwrite behavior remain intact.

A registered source-copy fixture observes full-span clears on real isolated
file operations and injected failures: partial/short reads, EOF errors,
zero/oversized/interrupted reads, all close positions, sync failures, malformed
records, internal/public capacity refusal, pending mismatches and committed
competitors. Failure outputs preserve their bytes, length and pending flag.
The strengthened fixture fails against the preceding storage source before it
creates a temporary directory. The existing storage-fault fixture also passes.

Full safety passes 100/100 Clang wallet CTest cases in 83.58 seconds and 99/99
optimized GCC cases in 119.77 seconds, with both analyzers and complexity caps
green. A 61-second bounded storage fuzz run completes 139,447 executions with
no finding. Android/JVM tests, both ABI builds, debug/release APKs and lints,
fixture isolation, 16 KiB alignment, architecture and doc counts pass. All eight
API35 storage/recovery instrumentation tests pass in 0.259 seconds using only
invocation-owned temporary directories and public GCM vectors. This does not
qualify hardware authentication, physical power loss or same-UID tampering.

Evidence is under
`apps/zcl-wallet/.cache/android-wallet/mission-20260917/storage-retirement/`.
Four older shorthand filenames in the security review were expanded into their
actual `.c` and `.h` names; its path-check findings are resolved. The global
path check still flags other historical upstream citations and unrelated files;
repository-wide lint is not green. TLS and all production boundaries remain
unchanged. This batch was pushed and remotely verified as
`dcde9d657e6c6e9b0d8991ccb86dfd502b4a8fab`.

The next batch retires the remaining parsed-record copy in change-custody
preparation, validation-only copies in all five change-storage entry points,
the exact-wallet comparison buffer, and local observation/recovery snapshots.
Append/repair retire compared snapshots before writes. No authentication,
index-consumption, output-publication, file-preservation or status rule changes.

A shared source-only observer now brackets preparation in the creation/
reservation, recovery and ownership fault suites. It requires parsed-record
retirement before preparation returns and full erasure of the exact enclosing
custody owner before reset/reuse. The preceding fixture allowance accepted
owner-sized wipes without requiring them. Three separate builds using callers
from `8828c2219` fail the new owner-retirement assertion, and the inherited
preparation source independently fails its parsed-record assertion. All four
negative checks fail before filesystem setup. Direct regressions cover NULL
inputs/owner, short record, unsupported version and entropy-length mismatch.

The registered change-storage retirement fixture checks exact outputs, canaries
on failure, stale append/repair refusal, failed IO, preserved partial records,
and actual whole-object zeroing. A test-only write interception requires consumed
comparison scratch to be retired before append/repair writes. Its inherited
storage-source build fails the cleanup assertion before filesystem setup.
These observers are excluded from Android libraries and operate on public
fixtures; they retain no live stack pointer after retirement.

Full non-TLS safety passes 101/101 Clang wallet CTest cases in 83.91 seconds and
100/100 optimized GCC cases in 120.84 seconds. Both analyzers and complexity
caps 10/15 pass. Separate bounded 61-second fuzz runs complete 90,352
change-storage and 17,923 review-wallet executions without a finding. Android/
JVM tests, both ABI builds, debug/release APKs and lints, fixture isolation,
16 KiB alignment, architecture and doc counts pass. All eleven API35 native
storage/recovery tests pass in 0.347 seconds, including fresh paired creation.
Evidence is under
`apps/zcl-wallet/.cache/android-wallet/mission-20260917/change-retirement/`.

The earlier full root-lint run has now terminated with fourteen failing gates,
including node/tooling checks, historical provider/upstream documentation paths,
and four standalone node-tool link targets. Its complete log remains under
the record-retirement evidence directory. No global pass or production custody
acceptance is claimed. This batch was pushed and remotely verified as
`5f6271da1c483aa2573c5ef840e681ff17306a11`.

A fresh archive of that exact checkpoint and its pinned source dependencies
built the unsigned release in a separate path, with all 57 Gradle tasks executed
and no copied build outputs. Its complete 629,847-byte APK compares identical
to the working-tree artifact: SHA-256
`762484d41084529b4c1d8bc38c88891ffd1db969213c383ec7c13fa97c9a2fab`.
This is same-host/toolchain, two-path unsigned reproduction, not independent
hardware, signed-release or custody qualification. The source identity and
build log remain in the change-retirement evidence directory.

Continued into the transaction-review receiving-input ownership boundary.
Its committed-record comparison now clears the entire 140-byte scratch on
every read outcome before returning, including partial dirty failures,
pending records, excessive lengths and byte mismatches. The existing
source-copy fault fixture checks actual erasure before RNG/derivation and
before enclosing-work retirement. All five entropy lengths, both chains,
caller-input mutation and unchanged-review assertions remain. The inherited
implementation fails the new retirement assertion before derivation.

The three focused review-wallet tests pass under both Clang's safety profile
and optimized GCC. Both exact-unit static analyzers and whole-source/test
complexity caps 10/15 pass. A bounded 61-second ASan/UBSan review-wallet fuzz
run completes 18,706 executions without a finding. Android/JVM tests, both
ABI builds, debug/release APKs and lints, fixture isolation and 16 KiB alignment
pass. The preceding full 101/100 safety suites remain checkpoint-specific;
they were not relabeled as a full run for this narrow follow-up. No new JNI or
hardware-custody behavior is claimed. Evidence is under
`apps/zcl-wallet/.cache/android-wallet/mission-20260917/review-record-retirement/`.
TLS, consent, authenticated chain-state and production boundaries are unchanged.
This follow-up was pushed and remotely verified as
`56190db1b5b5c96cc30bbe8b8fe870cd376bf580`.

Continued into Activity destruction. The previous sequential cleanup could
skip the secret-owning session and timer if prompt cancellation threw, or leave
the timer installed if secret-view clearing failed. Destruction now first
detaches the session and refuses further foreground work, attempts every owner,
and always attempts framework destruction. It preserves the first owner failure
without allocating suppressed-exception storage and tolerates partial creation.
Active worker entropy still clears only after worker termination.

Three initial device regressions fail against the prior controller at their
specific cancellation, timer-order and partial-creation assertions. The expanded
fixture also checks simultaneous cancellation/view failures, duplicate teardown,
actual Android destruction callbacks, concealed/erased public marker words and
worker-owned marker entropy remaining intact until termination. Its controller
is never launched: an inert application/test context supplies framework lifecycle
dependencies, and the existing storage-free host displays public markers only.
The first fixed-code run exposed missing fixture framework dependencies; those
were supplied without weakening any cleanup assertion or changing production
authentication. No key, real seed, wallet storage operation or prompt is used.

All 32 selected lifecycle/authentication/display tests pass on API35 in 15.085
seconds. The four new destruction cases also pass on API30 in 43.528 seconds.
The first API30 32-case attempt exceeded its 60-second command bound and is
not a completed suite pass. Core and debug/release Android JVM tests, both ABI
builds, debug/release APKs and lints, fixture isolation, 16 KiB alignment,
architecture and documentation counts pass. Native C was unchanged in this
lifecycle slice; prior sanitizer/fuzz results retain their stated scope.
The optional API36 four-case attempt exceeded its 90-second bound after two
reported passes; no completed API36 run is claimed for this change.
Evidence is under
`apps/zcl-wallet/.cache/android-wallet/mission-20260917/destroy-retirement/`.
These are public synthetic cleanup tests, not positive hardware authentication
or custody acceptance. TLS remains quarantined; global repository lint retains
its documented unrelated failures. Continue with native review-state lifetime
and transaction failure-atomicity inspection.

The lifecycle slice was pushed and remotely verified as
`8aa668537776e2a8951c13f0c481f52e3d3861cb`. Continued immediately into native
unsigned-review preparation. Opening now clears its staged review on every
entered exit, preparation clears its parsed transaction, and snapshot
publication clears its temporary value after transfer. The existing separate
stack frames, immutable draft, status precedence, fixed deadline and issuance
rules are preserved; no signing, custody or network authority is added.

A registered source-copy fixture observes full erasure with the real secure-zero
primitive while objects are live. It injects dirty partial parser, assessment
and serializer refusals, verifies unchanged owner/ID outputs, and mutates the
borrowed wire after parsing to check exact publication from the owned value.
Signed-input refusal, admission limits, snapshot canaries, stale IDs and
inclusive expiry remain covered. The inherited implementation fails the first
retirement assertion; the fixed fixture and existing review suite pass.
Optimized GCC stack reports retain bounded 3424-byte opening, 2288-byte
preparation and 1424-byte snapshot frames under the 4096-byte frame gate; these
figures do not bound nested call-chain usage. Explicit hazard review is in
`C_SAFETY_REVIEW.md`.

Full non-TLS safety passes 102/102 Clang tests in 84.48 seconds and 101/101
optimized GCC tests in 120.95 seconds. Both analyzers and production/test
complexity caps 10/15 pass. A bounded 61-second ASan/UBSan review-state fuzz
run completes 253,714 executions without a finding. Core/Android JVM tests,
both ABI builds, debug/release APKs and lints, fixture isolation, 16 KiB native
alignment, architecture and documentation counts pass. All thirteen API35
unsigned-review/lifecycle/render-failure tests pass in 5.717 seconds.
Evidence is under
`apps/zcl-wallet/.cache/android-wallet/mission-20260917/review-state-retirement/`.
Next: retire JNI snapshot/wire output copies under the existing VM fault and
replacement-owner tests. TLS and all custody/consensus authority remain unchanged.

The native review-state slice was pushed and remotely verified as
`72aad204f1ac4f485699243a1cb08ade3c59220b`. The JNI continuation now clears its
copied snapshot before unlocking and VM allocation, then erases the full
numeric/wire output capacity after attempted publication and before failure
cancellation. No JNI signature, review identity, packet format or permission
changes. Temporary transaction data is public; managed/VM erasure is not claimed.

The source-copy fixture observes live erasure with the real primitive, including
unused capacity, NULL/pending entry refusals, stale/expired IDs and maximum
packets. It adds NULL-without-exception and non-NULL-with-exception allocation
faults alongside existing NULL/exception and transfer failures. All four outcomes
are also interleaved with replacement ownership for both response shapes; an
old failed publication cannot cancel its replacement. The inherited source
fails the pre-allocation snapshot-retirement assertion.

The first expanded fuzz run exposed a test-oracle mismatch: it still assumed
every NULL result carried an exception after the fixture gained an explicit
NULL-without-exception mode. The oracle now predicts both result and exception
from the exact injected fault, rather than dropping its failure check. The
three-byte input is preserved in ignored local evidence and passes replay.
A subsequent bounded 61-second ASan/UBSan campaign completes 31,759 executions
without findings. Test/fuzz observers retain only numeric identities after a
native span's last use; no dead stack object is inspected.

Focused JNI/core/retirement tests pass on Clang and optimized GCC, and both
exact-unit analyzers plus whole-source/test complexity gates pass. Optimized
JNI snapshot/wire frames remain 3632/2000 bytes under the 4096-byte frame gate.
Core/Android JVM tests, both ABI builds, debug/release APKs and lints, fixture
isolation and 16 KiB alignment pass. All thirteen API35 unsigned-review,
lifecycle and render-failure tests pass in 5.412 seconds. Evidence is under
`apps/zcl-wallet/.cache/android-wallet/mission-20260917/jni-review-retirement/`.
The complete native safety gate passes: 102/102 Clang tests and 101/101 optimized
GCC tests, with TLS review OFF and analyzers/strict warning gates unchanged.
Next: retire JNI storage path, ciphertext-record and read-packet copies across
VM read/publication failures, retaining the existing fresh-entropy guarantee.

The JNI review slice was pushed and remotely verified as
`de59fd23c71b7fdf0e46b55fb8e155bbf59a884e`. Continued into JNI storage immediately.
Read now clears its copied path before Java allocation and its full native
packet on success or read/publication failure. Create/promote clear both copied
inputs on all returns; fresh paired creation retains entropy-first cleanup,
then retires record/path. Storage status precedence, exact-record checks,
authentication requirements, private packet format and no-overwrite are unchanged.

The expanded source-copy fixture observes all four buffer capacities while
live with the real primitive, including partial reads, pending/NULL/oversized
inputs, full-width nonzero entropy and packets, all four New/Set publication
fault classes, exact-record promotion refusal, corrupted storage and independent
VM output bytes. The inherited source fails its first fresh-copy cleanup check.
The fixture-isolated fuzzer observes path/record/entropy retirement too. Its
initial expanded entry exceeded the test complexity cap; a separate observer
reset helper restores the cap without removing assertions or suppressions.

Focused host JNI/fuzz-regression tests and exact-unit Clang/GCC analyzers pass.
Optimized read/write/fresh frames are 1248/1264/1328 bytes, under 4096. Android
JVM tests, both ABI builds, debug/release APKs and lints, fixture isolation and
16 KiB alignment pass. API35 storage and recovery instrumentation passes all
eleven cases in 0.386 seconds; all records/keys are public isolated fixtures.
Full native and final post-refactor fuzz results are recorded before committing.
Evidence: `apps/zcl-wallet/.cache/android-wallet/mission-20260917/jni-storage-retirement/`.
Next inspected gap: JNI draft numeric/address/wire scratch and its temporary
transaction are not retired, despite existing copied-input heap retirement.

## Current continuation — 2026-09-16

Worktree: `/root/z23-android`; branch:
`agent/android-jni-secret-retirement-20260915`. GitHub development remote:
`wallet-backup` (`CesareFI/zclassic-android-wallet`). Main is not a push target.
The inherited baseline is `d3245b900dfb7a864e5dc612479d5fe3bf5ae804`;
37 wallet-only commits follow the previously advertised backup `df3be5659`.
The inherited Electrum test changes were saved in Git stash before validation.
No history was reset or merged; current origin/main was fetched and inspected.
Its node-only work is outside this app mission and the app contract prohibits
merging. The installed main-only hook conflicts with development-branch backup;
the existing documented per-command backup exception leaves hooks unchanged.

Verified GitHub checkpoint: `29ea242380e6e169cd343f5cfc8445ebbdb88529` on the
same-named development branch. Direct remote advertisement matches local Git;
the branch now tracks its own fetched remote ref. All inherited commits are
preserved remotely. The hook exception affected only that development push.

That checkpoint adds stronger Electrum framing invariants and a reproducible
1,600-case host regression/corpus generator. Clang and optimized GCC sanitizer
suites pass 99/99 and 98/98 native CTest cases (81.72/118.35 seconds). Clang/GCC
analysis and production/test complexity caps 10/15 pass. A seeded ASan/UBSan
campaign completed 28,752 executions in 121 seconds, max input 16,385 bytes,
five-second input deadline and 512 MiB RSS cap (observed 258 MiB), without a
finding. Separate byte-corruption and completed-state-mutation candidates each
abort on the new invariant. The first completed-state mutation failed compilation
because it attempted to modify a const pointer; the final isolated mutation
uses the mutable feed input. No mutation changed the checkout's product source.

Android/JVM tests, both-ABI builds, debug/release lint, fixture isolation, native
alignment, architecture and whitespace checks pass. Root lint initially fails
two gates: a historical wallet-test count was mistaken for the node count and
the unrelated flag-registry self-test refuses an empty scan. The historical
wording now explicitly identifies wallet CTest cases; `make check-doc-counts`
passes. The flag-registry failure remains open; no global lint pass is claimed.
Evidence: `.cache/android-wallet/mission-20260916/`; durable test sources and
this summary accompany the source commit. No production code changed.

Next: explicit host-only investigation of the quarantined TLS certificate
finding under the current mission's network/TLS validation scope. Quarantine
and Android/JNI exclusions stay in place. Positive hardware custody, physical
camera acceptance, authenticated send/shielded completion and real sync remain
unfinished. TLS remains quarantined under `TLS_REVIEW.md`; it was not enabled
or investigated. Continue useful work after each verified development push.

2026-09-16 continuation with TLS explicitly parked: sync request/reply JNI
entries now refuse a missing environment or pre-existing VM exception before
locking or mutating the active owner. Previously, request refusal converted the
condition into resource exhaustion and reply refusal failed the attempt as an
invalid argument. The new fake-VM regression reproduces the request defect on
the inherited implementation and requires the same token, clock, deadline and
waiting state after NULL and pending-exception request/reply calls. A valid
version exchange then continues on that exact attempt. Normal malformed input,
allocation failures, explicit cancellation and cleanup behavior are unchanged.

The focused sanitizer regression passes. Full non-TLS C safety passes Clang
99/99 groups in 81.88 seconds and optimized GCC 98/98 in 118.42 seconds, with
Clang/GCC analysis and unchanged production/test complexity caps 10/15. The
rebuilt host JNI/JVM checks, both Android ABIs, debug/release APKs and lints,
fixture isolation and native alignment pass. No emulator was rerun because a
Java caller cannot normally enter a native method with an already pending VM
exception; existing real-JVM sync fixtures remain applicable. The complete
hazard review is in `C_SAFETY_REVIEW.md`; evidence is under
`.cache/android-wallet/mission-20260916/jni-sync-pending/`. TLS remains excluded
and quarantined. Next: continue independent native/JNI ownership auditing or
the bounded transparent-send composition prerequisites without enabling a
network source or spending route.

2026-09-16 continuation with TLS still parked: the public native sync abort and
watch-fail boundaries now reject negative and above-range `zcl_status` values
without changing their caller-owned state. Previously a direct abort stored an
impossible fault code, and the watch wrapper additionally terminated the live
attempt. Exact before/after regressions reproduce both inherited failures and
cover both ends of the invalid domain; valid reasons and stale-token precedence
remain unchanged.

The focused sync tests pass 2/2. Full non-TLS C safety passes Clang 99/99 groups
in 81.98 seconds and optimized GCC 98/98 in 118.72 seconds, with both analyzers
and unchanged production/test complexity caps 10/15 green. Host JNI/JVM tests,
both Android ABIs, debug/release APKs and lints, fixture isolation and native
alignment pass. The complete hazard review is in `C_SAFETY_REVIEW.md`; evidence is under
`.cache/android-wallet/mission-20260916/sync-status-domain/`. Next: continue
local C/JNI state and ownership auditing; do not enter the quarantined TLS path.

2026-09-16 continuation with TLS still parked: `beginSyncAttempt` now refuses a
missing JNI environment or pre-existing VM exception before locking, starting
an attempt or consuming its sequence. Previously either condition could mutate
the native owner despite VM refusal. Fresh-owner regressions cover NULL and
pending-exception calls, require byte-for-byte idle state with no added local
reference, and prove the next valid begin still receives token 1. Explicit
fail/close cleanup remains unchanged.

The focused JNI sanitizer regression passes. Full non-TLS C safety passes Clang
99/99 groups in 82.26 seconds and optimized GCC 98/98 in 118.48 seconds, with
both analyzers and production/test complexity caps 10/15 green. Host JNI/JVM
tests, both Android ABIs, debug/release APKs and lints, fixture isolation and
native alignment pass. The complete hazard review is in `C_SAFETY_REVIEW.md`;
evidence is under
`.cache/android-wallet/mission-20260916/jni-sync-begin/`. Next: continue local
JNI/native ownership auditing without entering the quarantined TLS path.

2026-09-16 continuation with TLS still parked: internal change-custody helpers
now reject NULL wallet, state, index and address arguments before dereference or
blinding generation. The focused change regression covers every invalid pointer
combination while valid reservation/recovery behavior remains unchanged. Full
non-TLS native safety passes Clang 99/99 and optimized GCC 98/98; the complete
hazard review is in `C_SAFETY_REVIEW.md`, with evidence under
`.cache/android-wallet/mission-20260916/change-custody-null/`.

2026-09-16 continuation with TLS still parked: change-custody callers now
retire their stack-owned custody work object on every return. The helper's
borrowed entropy pointer, copied change record and metadata are wiped through
one NULL-safe `zcl_change_custody_clear` boundary after create, reservation,
recovery and ownership paths finish. Fault-injection wrappers were updated to
recognize and verify this owner-sized wipe while retaining their existing
blinding-buffer assertions. The focused change suite passes 21/21; full
non-TLS safety passes Clang 99/99 and optimized GCC 98/98, with analyzers and
complexity caps green. Android host/JVM, both ABIs, APK builds, lints, fixture
isolation and native alignment pass for this slice. Evidence is under
`.cache/android-wallet/mission-20260916/change-custody-clear/`. No consensus,
transport or TLS code changed.

2026-09-16 continuation with TLS still parked: wallet-record JNI packing and
unpacking now retire all native header, IV, ciphertext, serialized-record and
parsed-record buffers on success and every refusal path. This closes a secret
residue window for encrypted wallet material while preserving the Java output
before native cleanup. The focused JNI-record test passes; full C safety passes
Clang 99/99 and optimized GCC 98/98, and Android host/JVM, both ABIs, APK
builds, lints, fixture isolation and native alignment pass for this slice.
Evidence is under `.cache/android-wallet/mission-20260916/wallet-record-clear/`.

## Historical checkpoints

Started 2026-09-11 18:11 UTC; requested work window ends 2026-09-12 14:11 UTC
(16:11 Europe/Amsterdam). Branch `agent/android-wallet-20260911`, based on
Z23 `337f4e6da1368087fa56e1a6cd30f2371a6041a6`.

The tmux session `zcl-android-20h-20260911` hosts persistent development commands.
It preserves shell/build processes across terminal disconnects; it does not
itself guarantee that an interactive reasoning service remains available.

Initial inspection: no existing Android project or Java toolchain. This app
owns `apps/zcl-wallet/`; sealed consensus sources and existing wallets remain
outside its write scope. Main checkout was clean at start.

Next acceptance: a reproducible debug APK plus core tests for checked amounts,
Zclassic transparent addresses, QR input bounds and malformed input rejection.
No completed test or custody claim is recorded before execution.

2026-09-11 18:45 UTC: first debug APK built in tmux (10 MB, unminified).
12 core tests passed: amount arithmetic/formatting, pinned public address
scripts, checksum mutations, network separation and bounded Base58 round trips.
Architecture placement passed. Android lint initially rejected two old test
dependency versions and a missing icon; fixes are in progress. No on-device
key operation, network sync or transaction is implemented at this checkpoint.

2026-09-11 18:51 UTC: user changed the implementation to C11/C17 core with a
thin Android adapter and explicitly prohibited Rust. No Rust project files or
toolchain were installed. An unsuccessful installer had created bootstrap
files; these were moved out of the user tool paths into a temporary quarantine.
Retain the working Android project and adapt all 16 passing Kotlin parser
tests and their public fixtures to C/JNI. Apply the user's full C safety review
before every C implementation commit.

2026-09-11 19:10 UTC: portable C17 amounts, Base58Check and transparent address
validation pass all 16 adapted JVM/JNI tests with `-Xcheck:jni`. Native C tests
pass nine groups under ASan/UBSan/leak checking, including the upstream hash
primitive known-answer tests. Clang static analysis of these core sources
reported no findings. The Android C/JNI build and lint passed for ARM64 and
x86-64 before the latest codec migration; that build will be rerun with codecs.
The bounded amount fuzz campaign completed 41,908,413 executions in 601 seconds,
without a reported finding. These are parser observations, not custody or
transaction compatibility acceptance. QR parsing is the remaining Kotlin logic
being moved into the C core before the first implementation checkpoint.

2026-09-11 19:56 UTC: the C parser checkpoint passes the full safety script:
Clang static analysis, GCC analyzer, strict C17 warnings, authored function
complexity at most 10, pinned provider hashes, ASan/UBSan and leak checking.
The three native executables contain 13 native test sections and also pass GCC Release.
All 16 Kotlin/JNI tests pass. A clean Android build passes debug/release assembly
and lint for ARM64/x86-64, with dependency lockfiles and SHA-256 verification
metadata generated. Debug APK: 3,159,115 bytes. The much smaller unsigned release
APK strips currently unused adapters; it is not a measurement of a complete
wallet. No device acceptance is claimed.

The exact final parser sources were fuzzed with bounded ASan/UBSan jobs in tmux:
amounts 4,169,736 executions/61 seconds; codecs 360,623/181 seconds; payment URIs
818,464/601 seconds. No finding was reported. Earlier codec fuzzing separately
completed 870,764 executions/601 seconds. Local logs and source/binary hashes
remain under ignored `.cache/android-wallet` and `native/build/fuzz-checkpoint`.
These counts are execution evidence, not a guarantee against defects.

Next milestone: C recovery/key derivation with independent known-answer vectors,
then authenticated on-device secret storage and the receiving/read-only UI.
The current launcher is a development placeholder. No private-key API, signing,
network synchronization or shielded transaction capability is enabled yet.

2026-09-11 20:52 UTC: C English BIP39 recovery, BIP32 derivation, OS randomness
and Zclassic BIP44 receiving addresses pass the published vectors and 48
independent OpenSSL comparisons. Provider errors, allocation failure, invalid
child keys, RNG failures and output preservation have explicit tests. The full
C safety script now also analyzes JNI, and passes its complexity/static-analysis/
ASan/UBSan/leak checks. All 20 JVM/JNI tests passed before a small test-cleanup
improvement; the Android checkpoint build will rerun them with that improvement.
The 15-minute recovery fuzzer completed 14,014,058 executions in 901 seconds
without a finding. libsecp256k1's separate 123-case suite and Valgrind constant-time
executable passed; the latter reported zero memory errors and no leaked blocks.

libsecp256k1 0.8.0's release signature verified against its listed maintainer key.
Only JNI entry points are intended to be dynamically exported. Key functions
are not exposed by the launcher yet. Next: authenticated Android storage,
recovery lifecycle and receive/QR/read-only network UI. Emulator installation
is complete; hardware Keystore acceptance still requires suitable hardware.

2026-09-11 20:56 UTC: the Android key checkpoint passes all 20 JVM tests,
ARM64/x86-64 debug assembly and Android lint. The debug APK is 3,643,616 bytes.
Dynamic-symbol inspection confirms only the 13 intended JNI exports. Repository
lint passes all 32 gates. The key safety review is recorded in
`C_SAFETY_REVIEW.md`; storage and UI are the next implementation slice.
All nine unsanitized native test executables additionally pass Valgrind with
origin tracking and full leak checks, with no reported errors or leaked blocks.

2026-09-11 22:45 UTC: bounded C wallet-record serialization and private-file
storage pass the full C safety script (13 native executables, both static
analyzers, strict warnings, complexity at most 10, ASan/UBSan/leak checking).
The four new native record/storage executables also pass Valgrind origin/leak
checks; normal exits retain only standard descriptors. Fault tests cover short
IO, bounded interruptions, failures at each flush/rename/close stage, five
process-death points, twelve competing creators, and a destination created
between the absence check and the actual kernel no-replace operation.

The first Android storage acceptance found that hard-link creation returned
EACCES. This candidate was replaced with `renameat2(RENAME_NOREPLACE)` and the
pending directory entry is flushed before rename. All affected checks were
rerun. The API-35 x86-64 emulator now passes the native-storage/GCM integration
test (7.801 seconds): create/read/no-overwrite, recovered-address agreement,
GCM rejection after changing each of 80 header bytes, and ciphertext tampering.
It uses public test fixtures and a public test AES key, not a production wallet
or a claim of hardware Keystore acceptance. The temporary diagnostic was removed.

All 25 JVM/JNI tests and ARM64/x86-64 Android debug assembly/instrumentation
assembly/lint pass. Debug APK: 3,679,016 bytes. Host dynamic-symbol inspection
shows only 20 intended JNI exports. Repository lint passes all 32 gates. The
full per-hazard source review is in `C_SAFETY_REVIEW.md`; format, ownership,
durability assumptions and recovery limits are in `WALLET_RECORD.md`.

The record-parser fuzzer completed 23,906,974 executions in 901 seconds without
a reported finding. Its exercised record/header parser source was unchanged
by the subsequent filesystem correction. Exact snapshot and binary hashes are
under `native/build/fuzz-record`; this is parser evidence, not filesystem fuzzing.
Local build/analysis/fuzz/Valgrind logs remain under ignored
`.cache/android-wallet` and native build directories.

Next milestone: the Android hardware-keystore adapter, per-use authentication,
secret lifecycle and create/restore/receive UI. The launcher remains a placeholder;
network sync, balance display, camera/QR UI, signing and shielded transactions
are not enabled yet. There have been no pushes, merges, production services,
mining, funded-wallet operations or Rust installations.

2026-09-12 13:06 UTC: the launcher now implements the create/restore/backup
confirmation/lock/unlock/receive workflow. The platform adapter requires an
actual provider-generated AES-256 GCM key with per-use hardware-enforced
authentication, checked by a C predicate. Backup confirmation and all wallet
decisions remain in C. UI-owned phrase arrays are bounded and cleared on
transition; a single bounded worker owns transient platform entropy. The
per-hazard review precedes the pending implementation commit.

The API-35 emulator became unresponsive and was restarted with its data
preserved, four virtual cores and 4 GB RAM. One host tool call took almost
12 hours to return, so this was not 20 hours of uninterrupted active coding.
The post-restart launcher also crashed; after stopping that development
launcher, the two Keystore tests passed in 3.330 seconds. Actual metadata is
software security level 0, hardware authentication false, key size 256,
authentication methods 3 and duration 0. The C predicate correctly refuses it.
This emulator cannot qualify successful hardware-authenticated create/restore
or unlock. The positive interactive cases remain unqualified; a dedicated
unsupported-protection UI case passed in 37.895 seconds. It verifies that the
actual generated provider key fails the C predicate, no recovery view appears,
no wallet record is written, and only that invocation's key/files are cleaned up.

Receiving QR generation now uses a pinned C provider with no heap allocation,
exact public address/network validation, fixed buffers and a white quiet zone.
ZXing was removed from the app runtime and is used only as a host-test decoder.
All 48 public fixtures decode directly from their C modules; all 288 rendered
scale/orientation cases decode with the multi-candidate image detector. A
separate regression preserves the simple detector's rejection of a valid
symbol. This is not real-camera acceptance; details are in `RECEIVING_QR.md`.

Current checks pass: 32 JVM/JNI tests (JNI checking enabled), two worker unit
tests, ARM64/x86-64 debug and instrumentation assembly, Android lint and all
32 repository lint-fast gates. The full C safety script passes 16 native
executables, Clang/GCC analysis, strict warnings, complexity at most 10 and
ASan/UBSan/LSan. The unchanged provider passes 521 upstream tests under
sanitizers. The QR fuzzer completed 14,331 executions in 301 seconds with no
finding; its source/provider/binary hashes and final source recheck are kept
under `native/build/fuzz-receive-qr`. The earlier confirmation fuzzer completed
11,088,373 executions in 901 seconds. Dynamic exports are only 23 JNI entries.

`NEXT_MILESTONE.md` lists the exact custody qualification and remaining phase-1
acceptance, followed by the original ordered send/shielded/mobile-validation/
separate-messaging scope. No balance or network verification is claimed. No
production node/miner, funded wallet, Rust installation, push or merge occurred.

2026-09-12 13:23 UTC: four Android tests pass in 42.989 seconds: the actual
Android Canvas output independently decodes at three aspect ratios on both
networks; an undersized view produces no cropped QR; recovery input remains
bounded and transfers/clears its array; recovery display clears owned arrays
including rejected input. These are public-fixture tests, not screenshots or
camera tests. Together with the two Keystore cases and one refusal workflow,
seven scoped device tests now pass.

Final review found that a fatal VM Error could bypass the worker's ordinary
Exception handler after setup entropy had been retained. The worker now clears
setup in finally whenever its action did not complete, before the Error
propagates; fatal errors are not swallowed. Debug assembly and worker tests
were rebuilt after this correction. Positive hardware workflows and fault
acceptance on real devices remain explicitly unqualified.

Unsigned release assembly with R8 and release lint also pass. Dependency locks
were regenerated across all configurations: ZXing is present only in host or
instrumentation test configurations, with no stale release/runtime entry.
The authored diff passes whitespace checking. The two vendored QR source files
retain upstream's whitespace unchanged and are verified by their SHA-256 pins;
no source-format normalization or generic lint weakening is applied.

2026-09-12 14:08 UTC: implementation checkpoint `66375d001` is committed locally.
Read-only source research found that the historical Electrum Zclassic client
advertises protocol 1.2, has a testnet header-verification early return and uses
fixed difficulty tables at some mainnet transitions. `READ_ONLY_SYNC.md` pins
the reference and records the original beta6 mainnet/testnet differences. This
is compatibility research, not network or mobile-validation acceptance.

The requested 20-hour window ends at 14:11 UTC. Host/tool delays prevented
uninterrupted active coding. The repository contains a development foundation,
not a completed or production-qualified wallet: hardware-positive custody,
camera scanning, read-only sync, sending, shielded operation, mobile validation
and messaging remain pending in the documented order. Only local development
artifacts are retained; no node, miner, real funds, Rust installation, push or
merge is part of this run.

2026-09-12 continuation after the requested window: added a bounded C receiving
request QR decoder and thin JNI adapter. The provider is pinned quirc with
explicit local hardening and a reproducible patch, not an unmodified upstream
release. Local probes reproduced the original NULL-source memcpy and
nonfinite-to-int UB before fixes. C validates image spans before allocation,
limits candidate/alignment/fitness work, refuses ambiguous/unsupported payloads,
and feeds decoded requests to the existing payment parser. Owned images and
decoded buffers are cleared; there is no camera, network or spending callback.

The final source passes 19 native executables with ASan/UBSan/LSan, authored
Clang/GCC analysis, provider Clang analysis and authored complexity <=10.
Allocation-failure tests verify unchanged output and zeroed allocations before
exactly-one free. All 38 JVM/JNI cases pass; six scanner cases use an independent
encoder. ARM64/x86-64 debug/test assembly, unsigned release assembly and Android
lint pass. The provider patch reproduces the compiled files exactly from the
pinned original sources and hashes. Dynamic exports are the 24 intended JNI
entry points.

The final fuzz run completed 12088 executions in 301 seconds without a finding;
source/provider/binary hashes recheck successfully under
`native/build/fuzz-scan/final`. An earlier run completed 11537 before the final
stack-buffer clearing change; it is not the final-source acceptance. Both new
Android public-fixture tests pass in 6.670 seconds using the verified development
test component. An initial invocation used the wrong component name and ran no
tests. One host response was delayed nearly two hours; work continued from
verified process/artifact state instead of restarting on stale observations.

The current slice implements decoding and its acceptance, not the camera UX.
Next add a bounded camera permission/lifecycle adapter and public-request review
screen, then qualify actual camera behavior. Hardware-positive custody and
read-only sync remain pending, followed by the original ordered phases in
`NEXT_MILESTONE.md`. No Rust, real funds, node/miner, push or merge was introduced.

2026-09-12 camera continuation: implemented the private scanner Activity,
Camera2/ImageReader lifetime adapter, bounded grayscale preview, isolated AIDL
decoder and full public request review. C validates/samples direct image planes
into <=147461-byte packets. One camera worker owns all Image/reader/device
access and closure. One frame may be outstanding; submission is capped at four
per second. Late OS camera-open callbacks retain at most one owner. The service
has no app permissions and checks its caller UID. Returned exact request bytes
are parsed again by C in the app process before display. Scan never authorizes
spending or enters the recovery/key workflow.

All 20 native test executables pass ASan/UBSan/LSan with authored GCC/Clang and
provider Clang analysis and authored complexity <=10. All 41 JVM/JNI tests pass.
The camera fuzzer completed 12070 runs in 301 seconds, reported RSS 263 MiB,
and final source/provider/binary checks passed. Debug/test and unsigned minified
release builds plus debug/release lint pass. Root lint-fast passes 32 gates.

On the API-35 software emulator, the isolated decoder's actual Binder UID is
different from the app UID; a public request round-trip and wrong-network
refusal pass in 23.136 seconds. Permission refusal returns no frame and closes
the worker in 0.642 seconds. Actual Camera2 frames plus three background/resume
cycles pass in 149.774 seconds: preview cleared, worker terminated, explicit
restart required. This is separate synthetic QR and real emulator capture
evidence, not physical QR-to-review or minified runtime acceptance.

Remaining scanner acceptance includes a QR through an actual camera into the
review screen, physical orientations, runtime permission-dialog behavior,
cancellation during pending camera open, process recreation and supported-API
coverage. Positive hardware custody remains unqualified. Bounded C read-only
sync is the next independent implementation milestone before transparent send,
shielded support, mobile validation and the separate messaging design.

2026-09-12 read-only protocol continuation: implemented portable C Electrum 1.2
request construction, reversed SHA-256 script hashes, bounded LF framing,
strict JSON reply handling, signed pending balance accounting, and mainnet/
testnet genesis identity/header serialization checks. Original beta6 C++
constructors/serialization generated the public genesis fixtures offline and
checked both original hashes before export; no node or proof bypass was run.
The profile accepts full DNS names in feature-host maps while bounding decoded
keys to 256 bytes, frames to 16384, tokens to 128 and nesting to eight levels.
Output arguments remain unchanged on failed replies. Reused independent JSON/
UTF-8 packages are pinned and included in native analysis and Gradle test input
tracking. Their Unicode-display decoding limitation is documented and excluded
from the protocol's ASCII adapter.

All 22 native executables pass ASan/UBSan/LSan, including the unchanged JSON
provider test suite. Authored GCC/Clang and provider Clang static analysis pass;
215 authored functions in 43 files have complexity <=10. The final tmux fuzz
campaign includes maximum-size frames and completed 275159 executions in 301
seconds, with RSS 258 MiB, no finding and matching final source/binary hashes.
The preceding campaign before the DNS-key boundary correction completed 471076
executions; its counts are not attributed to the final source. All 41 JVM/JNI
tests, Android debug build/lint and 32 root lint-fast gates pass.

This is protocol implementation, not completed synchronization. No endpoint,
address query, TLS connection, balance UI, signing or broadcast was enabled.
Next implement C TLS/socket lifetime, source trust, deadlines, notification/
reorg/retry state and Android unverified/offline/stale balance presentation.
Hardware-positive custody and the previously listed camera acceptance remain
pending; all later phases retain their original scope.

2026-09-13 continuation: preserved the existing eight wallet commits and all
uncommitted source. The original TLS reproducer/corpus, provider source, binary,
hashes, sanitizer output and analysis remain at the original artifact path;
a hash-checked second archive plus complete pre-edit diff/untracked snapshot is
under `.cache/android-wallet/continuation-20260912`. TLS is explicitly
**BLOCKED — REQUIRES FURTHER SECURITY REVIEW**. The investigation was not rerun.

Normal host/Android builds now exclude the transport and TLS provider units,
restoring the original hash-only runtime profile. Explicit host review is OFF
by default and forbidden with JNI/Android. Whole-archive checks verify absent
TLS symbols and required sync/hash symbols; the forbidden configuration refuses.
The safety script labels enabled-wallet versus explicit TLS-review scope and
retains all provider checks in the latter. Existing TLS findings remain open.

Completed validation of the pre-existing offline C sync state and added its
state-aware malformed-response fuzzer. Six requests enforce identity before
address disclosure and complete-only, equal-tip reporting. Wrong IDs, network,
notifications, changed tips, cancellation and timeouts discard candidate values.
No endpoint, spending or network authority is enabled.

Authored Clang/GCC and enabled-provider analysis pass; 276 authored functions
in 52 files satisfy complexity <=10 (including preserved transport functions).
All 23 native functional executables pass ASan/UBSan/LSan. The quarantine test
also passes after correcting its whitespace expectation for CMake's wrapped
error output. The first sandbox fuzz run completed 127462 inputs but failed
LSan process inspection at shutdown; it is preserved and is not called a pass.
The host campaign passed 128180 inputs in 121 seconds with final hashes matching.

Android debug/test assembly, debug lint, 41 core JVM/JNI tests and two executor
tests pass offline. Six API-35 emulator public-fixture tests pass in 37.421s:
native storage/GCM, receive QR canvas, scan QR and isolated IPC. These do not
qualify hardware custody. Root lint remains separately blocked: the old root
allowlist rejects environment-injected `.agents`/`.codex`; the unrestricted run
also reports an empty-scan failure in its flag-registry selftest. Neither gate
was weakened and neither directory was removed. Local logs retain both runs.

Next independent work is C deadline/freshness and late-result handling for
unverified balances, then fixture-only Android presentation and lifecycle
acceptance. No push, merge, upstream integration, production node or real funds
are part of this checkpoint; upstream divergence was inspected and retained.

2026-09-13 next slice: added the C foreground balance watch around the existing
sync attempt. It owns one selected source/address, one attempt token and one
cached unverified report. Deadlines are <=30 seconds; stale age starts at 60
seconds. Explicit failures, refreshes and offline/cancelled attempts retain only
a stale prior report. Backward monotonic time clears it. Late/completed tokens
cannot mutate another attempt or its clock; overflow and token wrap refuse.
Close/restart discards all state. The required caller owner-lifetime check and
eventual I/O interruption remain explicit adapter responsibilities.

All 25 active native tests pass ASan/UBSan/LSan, including the archive quarantine
check and new deadline/freshness/lifetime cases. Authored Clang/GCC and enabled
provider analysis pass; 288 functions in 53 files remain at complexity <=10.
The event-sequence fuzzer passed 76753 runs in 121 seconds and final source,
configuration, provider-manifest and binary hash checks match. Android debug
and unsigned minified release builds, both lints, core JVM/JNI and executor tests
pass. Root lint's previously documented environment/selftest blockers remain
unresolved; the TLS investigation remains parked. No network or new JNI API is
enabled by this slice. Next validate camera recreation independently, then
implement fixture-only Android balance presentation/lifetime bindings.

2026-09-13 camera follow-through: the new emulator-only Activity recreation
acceptance passed in 114.094 seconds with actual captured frames before and
after recreation. The old preview clears, the old worker terminates, the new
Activity requires explicit start and uses a distinct preview, then final
background cleanup releases its frame/worker. Debug test assembly and lint pass.
The architecture-tree gate also passes. This adds Activity recreation evidence,
not an OS-process-death or physical camera QR-to-review claim.

2026-09-13 authentication follow-through: code review found that the documented
90-second pending authentication continuation relied only on Handler uptime.
The C custody module now checks elapsed monotonic age, rejecting >=90000ms and
backward time. Android checks that predicate before opening the prompt, on its
successful callback, before foreground delivery and on resume. JNI refuses
negative timestamps before unsigned conversion. Hardware metadata/per-use
policy, same-Cipher checks and storage behavior are unchanged. Android's clock
contracts and the precise claim are linked in KEYSTORE_PLATFORM.md.

All 25 active native tests pass ASan/UBSan/LSan. Enabled-code static analysis
passes and 291 authored functions in 53 files remain at complexity <=10.
Timestamp metamorphic fuzzing completed 20564425 runs in 31 seconds; final
C/JNI/platform source, configuration and binary hashes match. Debug/test and
unsigned minified release assembly, debug/release lint, 42 core JVM/JNI tests
and two executor tests pass. Six emulator tests pass in 4.541 seconds: new
timestamp JNI boundaries, existing isolated Keystore refusal and secret-view
checks. The actual software-only key still fails the unchanged hardware policy
and refuses unauthenticated finalization. Actual successful hardware-prompt
suspend/resume remains unqualified; simulated timestamps do not prove it.

The whole-archive quarantine test additionally passes against the actual
ARM64 and x86-64 Android debug core/hash archives. The initial preservation
archive/diff checksums still match. TLS remains BLOCKED — REQUIRES FURTHER
SECURITY REVIEW; its original reproducer/corpus/provider source/output were
not altered or rerun. Root lint's preserved environment/selftest blockers
remain separate from passing wallet/Android gates. All checkpoints remain on
the existing wallet branch, with no push, merge, production node or real funds.

2026-09-13 scanner correctness follow-through: changing the network before
starting capture previously lived only in the radio group; recreation restored
the launch Intent's network. Selection now updates the Activity immediately,
and only its public mainnet/testnet preference enters saved state. Capture,
decoded requests and permission continuation remain unsaved; no camera or
network starts automatically after recreation.

The corrected public-state regression fails on the preceding implementation's
selected network in 32.265 seconds and passes after the fix in 46.999 seconds.
An initial test incorrectly treated RadioButton.performClick's listener return
as selection success; it was corrected to assert the actual checked state.
Before/after source and APK hashes are retained under the continuation cache.
Debug/test and unsigned minified release builds and both Android lints pass.
No C, JNI, custody, decoder or transport code changes in this slice. The next
implementation remains fixture-only Android balance presentation with the C
watch's owner/time contracts; physical hardware custody, TLS, real sync,
transactions and the documented camera/device acceptance remain unfinished.

2026-09-13 autonomous continuation: a queued recovery phrase previously stayed
in its owned character array after session closure until its UI callback ran.
The new bounded RecoveryPhraseDelivery owner clears that pending array at close,
including a worker submission racing closure. Closed callbacks cannot deliver;
queue rejection, excessive input and receiver failure also clear their input.
Successful UI delivery transfers cleanup to the secret view. No hardware policy,
record format or active durable-write behavior changes.

Seven deterministic delivery tests and both existing executor tests pass.
Three API-35 emulator secret-view tests pass in 4.205 seconds, including actual
main-executor queued delivery cancelled before the queue drains. Debug/test and
unsigned minified release builds and both Android lints pass. Evidence and exact
source/APK hashes are in .cache/android-wallet/48h-20260912T235829Z. No native C
changes required repeating the prior sanitizer/fuzz campaigns. This is owned
array lifetime evidence, not full VM erasure or positive hardware custody.
The 48-hour continuation/backlog is retained in that scratch directory. TLS
remains BLOCKED — REQUIRES FURTHER SECURITY REVIEW and disabled; the next
independent task is the bounded C lifetime owner for read-only sync JNI.

2026-09-13 C lifetime boundary: the new caller-owned sync pool holds at most four
public watches, assigns never-reused positive IDs that fit Java long, and clears
closed slots without resetting ID history. It refuses exhaustion and leaves
failed-open outputs/state unchanged. A borrowed watch remains inside a serialized
synchronous C call; asynchronous adapters retain IDs and look them up again.
The regression reuses a slot with the same numeric attempt token and proves the
old lifetime cannot reach or alter the new watch. No JNI/global registry or
transport is enabled in this slice.

The 25 existing active native tests pass ASan/UBSan/LSan. The new test initially
expected INVALID_ENCODING for a cross-network address; it was corrected to the
existing UNSUPPORTED contract and then passed the focused sanitizer run. Logs
preserve that initial test failure. Clang/GCC and enabled provider analysis pass;
296 authored functions in 54 files remain at complexity <=10. A bounded owner
event fuzzer completed 33105 runs in 61 seconds with matching final source/binary
hashes. Android debug and unsigned release assembly and the architecture-tree
gate pass. Evidence is under .cache/android-wallet/48h-20260912T235829Z. Continue
with the serialized JNI registry and fixture-only callback/deadline acceptance;
TLS and physical hardware qualification remain separately blocked.

2026-09-13 read-only JNI binding: an explicit four-slot process registry and
mutex now serialize C owner lookup/use/close. Positive IDs are never native
pointers. The thin managed owner retains each attempt's original owner, checks
native status values and closes explicitly. C owns protocol, limits, clocks,
signed balances and freshness. JNI clears/frees bounded reply storage even on
allocation/region exceptions; failed Java request delivery aborts its attempt.
No socket, endpoint, custody access or app balance display is enabled.

All 27 active native tests pass ASan/UBSan/LSan, including the new fake-VM
allocation/region-failure fixture and unchanged TLS quarantine test. Clang/GCC
and enabled-provider analysis pass; 309 authored functions in 55 files remain
at complexity <=10. JNI fuzzing through the fake VM completed 21184 runs in
91 seconds, with final source/binary checks passing. A subsequent CMake guard
also excludes the fake-VM fixture from explicit TLS-review configurations; after
reconfiguration the tested fuzzer binary remained byte-identical.

All 51 core JVM/JNI tests and nine app executor/delivery tests pass. Nine new
core cases cover complete mainnet/testnet exchanges, signed pending delta,
stale boundary, offline retention, rollback, owner/slot reuse, signed arguments,
capacity, malformed frames and direct concurrent JNI callbacks. The Android
fixture initially needed a Kotlin nullable-property compile correction; after
that correction both builds/lints pass, and the API-35 emulator's full public
fixture test passes in 1.904 seconds. Debug/test and unsigned minified release
assembly pass. The application APKs exclude sync fixtures; only the test APK
contains the twelve shared JSON frames. Architecture and final quarantine
checks pass. Logs, public corpus and exact hashes are retained in the 48-hour
scratch directory.

Next bind Android's foreground worker and main-queue callback lifecycle to this
owner, retaining explicit unavailable/stale/unverified presentation. A queued
snapshot still needs the enclosing foreground identity check before display.
Physical custody, real-network validation, TLS and transactions remain unfinished;
TLS is still BLOCKED — REQUIRES FURTHER SECURITY REVIEW, with evidence intact.

2026-09-13 fuzz build guard: a negative configure regression reproduced that
ZCL_FUZZ=ON/ZCL_SANITIZE=OFF was accepted, emitting coverage-only core commands
despite sanitizer flags on harnesses. That combination now refuses explicitly.
The positive regression inspects 56 emitted native/provider compile commands
for ASan/UBSan and fail-on-finding flags. It also detected that the standalone
JSON-provider test lacked the fail-on-finding option (its sanitizer flags were
already present); that option is now added. The three affected configuration,
provider and quarantine tests pass, including all eight provider cases. The
previous JNI fuzzer binary is byte-identical after reconfiguration. No production
C behavior or application APK changes in this slice. Initial/final configure
logs and command databases are preserved in the 48-hour scratch directory.

2026-09-13 serialized elapsed clock: the managed sync owner now samples its
required clock function after acquiring its monitor. Worker and display callers
cannot reorder timestamps sampled before waiting for that monitor. Android must
supply elapsedRealtime; C still owns deadline, freshness and rollback policy.
Close drops the clock reference, and closed owners never invoke it. The raw JNI
timestamp boundary fixtures are retained.

All 52 core JVM/JNI and nine app JVM tests pass; the added case verifies sampling
under the owner monitor and no sampling after close. Debug/test and unsigned
release builds and both lints pass. The API-35 mainnet/testnet sync fixture passes
in 1.746 seconds. The sync-clock logs and source/APK hashes in the 48-hour scratch
directory identify this evidence. No native C changed; prior native safety
evidence remains applicable. Next protect queued Android display updates from
stale snapshots and closed foreground owners.

2026-09-13 queued balance presentation: the Android adapter now owns its original
C sync lifetime and queues only redraw signals. It samples the snapshot when
the UI actually runs, coalesces pending signals, and drops callbacks/owner on
foreground replacement. Closed callbacks perform no clock read or render.
Snapshot failure closes the owner and reports a stable unavailable status;
receiver failure closes and propagates. No endpoint or balance screen enabled.

Nine new app JVM/JNI tests pass, alongside the nine prior app tests and 52 core
tests. They cover stale-at-delivery, offline refresh failure, rejection/retry,
owner/clock/receiver failures, concurrent signals and close during submission.
Both real Android main-queue tests pass on API 35 in 2.38 seconds, proving the
delayed freshness decision and old-callback cancellation on replacement.
Instrumentation was initially slow to produce output; its completed result is
green, with no retry or change to assertions. Debug/test and unsigned release
builds, both lints and the architecture-tree gate pass. Native behavior did not
change. Evidence and hashes are in the balance-presentation files in the 48-hour
scratch directory. Positive custody and TLS remain independently blocked.

2026-09-13 signed amount display: C now formats bounded signed changes with an
explicit sign for nonzero values. It checks +/- MAX_MONEY before negation, so
INT64_MIN and out-of-range values refuse without touching caller output. The
existing decimal formatter is reused; payment parsing remains nonnegative.
JNI shares one checked <=18-byte result copier. No floating-point or locale
conversion is used, and no transaction authority is introduced.

All 28 active native tests pass ASan/UBSan/LSan, including six amount groups.
Clang/GCC and enabled-provider analysis pass; all 312 authored functions across
55 files remain at complexity <=10. The expanded amount fuzzer checks signed
round-trips, arbitrary signed bit patterns, canaries and unchanged failure
outputs: 1611305 executions in 61 seconds, no finding, final hashes match.
All 53 core JVM/JNI and 18 app tests pass. The two API-35 read-only/amount tests
pass in 2.424 seconds. Debug/test and unsigned release builds and both lints
pass. The amount-delta logs, corpus and hashes are retained in the 48-hour
scratch directory. Next expose the next display-expiry delay from C, so an idle
foreground UI can invalidate its display without duplicating freshness policy.

2026-09-13 C expiry hint: snapshots now return a relative delay to an active
attempt deadline or fresh-report expiry, with zero for no pending timed change.
JNI projects ten checked longs and refuses NULL env before altering the watch.
The managed adapter retains the hint without reimplementing freshness policy.
No timer, endpoint, periodic poll or persistence is added in this slice.

The native run passed 27 tests but the extended watch fixture initially expected
a retry to clear its previous clock fault. C intentionally retains that fault
until complete success. The corrected expectation then passed focused sanitizer
validation, completing coverage of all 28 active tests. Production policy did
not change. Clang/GCC/provider analysis and all 312 functions/55 files <=10 pass.
The watch fuzzer follows each returned delay on a copied watch and checks both
sides of the exact transition: 47850 runs/61s. The changed JNI packet also passed
4961 fake-VM fuzz runs/31s. Final source/binary hashes match; no finding.

All 54 core JVM/JNI and 18 app tests pass; signed-clock-limit deadline/cancellation
and exact freshness hints are covered. Four API-35 sync/presentation tests pass
in 3.08 seconds. Debug/test and unsigned release builds and both lints pass.
Initial/final logs, corpora and hashes are retained in the sync-delay files in
the 48-hour scratch directory. Next bind the hint to one cancellable foreground
UI timer, then render explicit unavailable/stale/unverified balance views.

2026-09-13 owned UI wakeup: foreground presentation now owns one replaceable
main-queue timer using C's delay. Timer delivery only coalesces a new snapshot
read. Close cancels it and rejects captured late callbacks; no periodic polling
or automatic network retry is introduced. Failure to schedule a required wakeup
closes the owner and reports unavailable before publishing a new unverified
display. Handler scheduling addition is checked; elapsedRealtime in C remains
the time authority. Foreground replacement is still required across pause/resume.

Six new JVM cases cover early/late expiry, idle timeout, scheduling rejection,
captured callback after close, armed-timer cleanup on receiver failure and queue
coalescing/rejection. All 24 app tests and 54 core tests pass. Four API-35 tests
pass in 3.354 seconds, including an idle report crossing the exact C freshness
boundary through a real main-queue timer and cancellation of replaced callbacks.
Debug/test and unsigned release builds, both lints and architecture pass. No
native C changed. Logs and hashes are in the balance-wakeup files in the 48-hour
scratch directory. Next implement explicit address-balance rendering and prove
that view restoration cannot recover a previously displayed amount.

2026-09-13 explicit balance view: the receive screen now includes an unavailable
address-balance widget with no numeric amount. Public fixture reports render
C-formatted confirmed/total amounts and signed pending changes, always labeled
unverified and scoped to this address. Stale reports remain explicitly outdated
and unverified. Update/fault states never manufacture a zero balance. No endpoint
is connected. Formatting failure clears previous amounts before propagating.
The view saves no text and ignores even an earlier ordinary TextView's saved
state under the same ID; detachment clears the display.

Five API-35 tests pass in 6.558 seconds: exact signed display/offline retention,
unavailable versus reported zero, saved-state rejection, malformed-input clearing,
and real timer-to-view stale transition. Lint initially found assembled status
messages requiring resource placeholders; those messages were corrected without
a suppression. Initial and corrected logs are preserved. Debug/test and unsigned
release builds, both lints, 54 core/24 app tests and architecture pass. No native
C changed. Evidence/hashes are in balance-view-final files in the 48-hour scratch
directory. Next qualify these display owners through actual Activity recreation
and foreground replacement using a public fixture host, without wallet access.

2026-09-13 Activity lifecycle: a nonexported debug-only host now composes the real
receive screen, bounded C sync owner, presentation, wakeup and balance view using
only public fixture frames. It has no storage, Keystore, endpoint or intent-state
input. Pause/destroy close the original owner and clear amounts; resume creates
an empty owner with no automatic replay. Release source sets exclude the host.

All three API-35 cases completed with zero failures: five recreation cycles
(more than the four registry slots), background/resume with a live old attempt,
and a newly launched Activity after completion. The host-side 120-second adb
timeout ended before the final console result. The preserved process-specific
TestRunner log proves completion of all three cases over 128.082 seconds; the
original timeout record is retained, and no passing case was rerun. Debug/test
and unsigned release builds, both lints, 54 core/24 app tests and architecture
pass. APK manifests confirm a nonexported debug host and no release entry;
neither application APK contains sync response assets. Source/APK hashes and
both runner/host logs are in the balance-lifecycle files in the 48-hour scratch
directory. This does not qualify positive custody or process-death recovery.
Next make release fixture separation a repeatable build check, then continue
process relaunch and transaction/history correctness work.

2026-09-13 repeatable fixture isolation: Gradle now exposes checkFixtureIsolation
and includes it in the app check lifecycle. It builds/inspects debug, test and
unsigned release APKs, requires exactly one nonexported debug display host,
rejects that host in release, and requires all twelve response frames only in
the test APK. The regression injects exported-host and release-host manifest
output; both must fail for their specific reason. APK bytes are never modified.

The new task, both Android lints, shell syntax checks and architecture pass.
All prior lifecycle source/APK hashes still match, so no unchanged emulator
suite was repeated. Positive/negative reports and hashes are retained under
fixture-isolation in the 48-hour scratch directory. A read-only upstream fetch
advanced origin/main to a898cf77913f786e69c2e85b357004972d796976; the wallet
worktree was 24 ahead/16 behind before this checkpoint. No upstream changes
were integrated and no external push was made. Continue process relaunch and
C transaction/history work; TLS and hardware qualification remain blocked.

2026-09-13 explicit process relaunch: the bounded opt-in emulator controller
verified a public displayed report in PID 22911, confirmed that exact app PID,
force-stopped only the development app, and observed termination. Its next
instrumented process (PID 22968) started the fixture without an amount or replay
and began an empty protocol exchange. That test passed in 28.579 seconds.
Preparation was deliberately killed after its readiness assertions and is not
counted as a passing standalone test; the expected process-crash output and its
host status are retained. No wallet record, credential, key or endpoint was used.

Debug/test/release builds, both lints, APK-isolation positive/negative checks,
shell syntax and architecture pass. No application or native implementation
changed, so prior JVM/sanitizer/fuzz evidence remains applicable. The controller
requires a new report directory, explicit emulator serial, matching PID and
in-app emulator opt-in. Its preparation, termination, new-process success and
source/APK hashes are under process-relaunch in the 48-hour scratch directory.
This is clean process relaunch, not OS saved-task or positive-custody recovery
qualification. Continue with bounded C transaction-history parsing using the
pinned original Zclassic transport reference; no real source is enabled.

2026-09-13 bounded C history codec: added the original scripthash history
request and an allocation-free reply parser for at most 16 unique decoded
transaction IDs. Heights retain -1/0 mempool semantics and are bounded by
INT32_MAX; the local-only -2 sentinel refuses. Server order is preserved.
Oversized, duplicate, malformed and wrong-envelope replies leave output
unchanged, including failures after valid entries. Empty history remains only
a server assertion. The pinned historical synchronizer/network/wallet source
was fetched as text and hashed; no old client, wallet or endpoint was executed.

All 29 native ASan/UBSan/LSan tests are covered by passing runs. The initial
history test expected INVALID_ARGUMENT for ID zero; the shared envelope parser
correctly returns INVALID_ENCODING. That expectation was corrected, with the
initial failure retained and the complete history test passing on rerun.
Clang/GCC static analysis and 317 authored functions in 56 files at complexity
<=10 pass. The seeded Electrum campaign completed 64,372 executions in 91
seconds without a finding; exact source/fuzzer hashes match afterward.

Android debug/test/unsigned release assembly, APK isolation including negative
mutations, 54 core JVM tests, cached 24 app JVM tests, both lints and architecture
pass. No JNI entry or UI route to history was added; no unchanged emulator
suite was repeated. Evidence, initial failure, focused correction, corpus and
hashes are in history-* files in the 48-hour scratch directory. Next add an
explicit optional history exchange to the existing C sync lifetime while
preserving the six-response balance-only path and transport quarantine.

2026-09-13 optional C history lifetime: explicit start/init profiles insert
history between balance and final tip, while the existing default remains six
responses. The opt-in profile reserves seven IDs without wrapping. Reports
include history only after final height/hash equality and refuse positive
history heights beyond the initial tip. Every failed attempt clears candidate
history and balance; prior completed history remains explicitly stale during
failed refresh. Source/address, elapsed deadline, late-token, rollback and
empty restart rules are shared with the existing watch. No JNI history entry
point, endpoint or history display is enabled yet.

All 30 native ASan/UBSan/LSan tests pass, including failure/cancellation at
every phase, changed height/hash, both networks, maximum request IDs, explicit
empty history and synthetic matching positive-height assertions. Clang/GCC
analysis and 322 functions in 56 files at complexity <=10 pass. Bounded fuzz
campaigns completed 39,771 sync executions/91s, 52,521 watch executions/61s and
5,244 JNI executions/31s with no finding. Review of initial watch seeds found
their first snapshot/reset pair exercises rollback; a supplementary 1,000-run
campaign starts directly with complete mainnet/testnet history exchanges.
All final source and fuzzer hashes match; the earlier corpus is preserved.

Android builds for arm64-v8a/x86_64, 54 core and 24 app JVM tests, both lints,
APK isolation and architecture pass. The existing API-35 default-sync fixture
passes two tests in 2.08 seconds, retaining the ten-long JNI snapshot contract.
Measured host fixed storage is report 688, watch 1504 and four-owner pool 6056
bytes; no persistent allocation or worker was added. Logs, hashes, corpora and
size measurement are under history-sync/history-watch/history-jni in the
48-hour scratch directory. Continue with checked JNI history projection and
public fixtures, then pinned-original transparent transaction work.

2026-09-13 checked JNI history projection: `ReadOnlySync.withHistory` opts into
the same four-slot registry with nonreused owner IDs. One C snapshot supplies
balance/freshness/history atomically before Java allocation. The bounded packet
contains at most156 longs, encoding each transaction ID as eight exact unsigned
32-bit words; no unsigned64-to-signed conversion is used. The thin managed
decoder checks shape/count/word/height bounds and renders lowercase IDs. The
original constructor and ten-long balance snapshot remain compatible. No
history screen, endpoint or spending route is enabled.

All30 native ASan/UBSan/LSan tests, Clang/GCC analysis and330 functions/56files
at complexity<=10 pass. The fake VM covers maximum16-entry packets, high-bit
hash words, allocation/region exceptions, retained timeout on allocation failure,
NULL-env nonmutation and closed-owner replacement. The extended JNI fuzzer
completed17,337 executions in91s without a finding; source/binary hashes match.

All58 core and24 app JVM tests, arm64-v8a/x86_64 debug/release builds, APK
isolation, both lints and architecture pass. Three API35 tests passed in3.268s:
maximum history on both networks and late-callback rejection, plus the existing
balance/signed-amount regressions. New fixtures assert empty versus absent
history, malformed/oversized replies, shared capacity and ID limits. Existing
public assets are reused; no wallet, secret or remote query was involved.
Evidence is in history-jni-final/history-jni files in the48-hour scratch
directory. Continue with explicit unverified history display and lifecycle
fixtures, then original transparent transaction serialization/signing work.

2026-09-13 explicit history view: the receive screen now includes an unavailable
history section after lock/scan controls. Public fixtures render bounded IDs,
reported pending/block-height labels, and explicit unverified/outdated states.
Empty assertions differ from absent queries and unavailable reports; no amount,
confirmation count, explorer link or spending action is added. Invalid display
rows clear prior IDs before refusal, and save/restore/detach paths cannot revive
history text. Both public report views now blank existing text before formatting
their unavailable message. There is still no enabled network source.

The initial device run passed all five balance-view regressions but rejected
the new history class during JUnit initialization: an expression-bodied test
inferred ActivityScenario instead of Unit. Its return was corrected; the five
history cases then passed in42.288s, including actual detach and foreground
timer expiry. The original failure log is preserved and unchanged balance cases
were not rerun. Builds, both lints, APK isolation, app JVM tests and architecture
pass; the58 core JVM tests and native sanitizer/fuzzer evidence remain current,
with native source/fuzzer hashes unchanged. Source/APK manifests and logs are
under history-view/history-view-final in the48-hour scratch directory. Continue
combined balance/history Activity/process lifetime integration, then pinned
original transparent transaction work.

2026-09-13 combined report lifetime: one foreground snapshot renders balance
and history together, clearing both on malformed display data or a failed
render. The nonexported debug host has an explicit memory-only history profile;
replacement closes its prior owner. Pause clears both views, resume starts an
empty owner, and recreation requires a new explicit fixture exchange.

Four API35 lifecycle cases passed in153.158s, including five recreations beyond
the four-owner capacity, cancelled late attempts, background/resume, composite
render failures and the default balance background regression. The bounded
history process controller verified the requested profile and live PID before
termination, then proved empty views and a fresh request sequence in a different
process: one relaunch test passed in22.99s. No wallet or key was opened or erased.

The58 core and24 app JVM tests, both ABI builds, debug/release lints, APK fixture
isolation and architecture pass. Native source/fuzzer hashes are unchanged;
the existing sanitizer, analysis and fuzz evidence remains applicable. Logs,
PID records and source/APK hashes are in history-lifecycle-* and
history-process-run1 under the48-hour scratch directory. Continue with bounded
transparent transaction serialization/parsing and pinned-original fixtures.

2026-09-13 bounded transparent transaction codec: C now owns a fixed v4
transparent representation, canonical parser, transactional serializer and
SHA256d ID calculation. Eight inputs, sixteen outputs,128/25-byte script caps
and1925-byte wire cap bound memory/work; unsupported larger/legacy/shielded
forms refuse. Unique/non-null outpoints, expiry and checked output totals are
enforced, while scripts remain opaque and no funding/signing authority exists.

Three fixtures preserve transparent prefixes from pinned original beta6
sighash rows with explicitly replaced zero shielded tails. A read-only shell
projector records exact original objects and uses OpenSSL for independent IDs.
These projections establish byte/hash behavior, not original-node acceptance.
All fixture truncations, output capacities, max-size boundaries and malformed
counts/amounts/expiry/outpoints have deterministic failure-atomicity coverage.

All31 native ASan/UBSan/LSan tests pass, as do Clang/GCC analysis and348 authored
functions in60 files at complexity<=10. The wire/object fuzzer completed
2,471,229 executions in91s without a finding; source/binary hashes match.
The58 core and24 app JVM tests, both ABI builds, both Android lints, APK fixture
isolation and architecture pass. No JNI change or unchanged device suite was
rerun. Evidence/corpus/hash manifests are in transaction-* under the48-hour
scratch directory. TLS stays blocked/excluded. Continue with checked synthetic
funding, output classification, change ownership and exact-byte review binding.

2026-09-13 previous-output and destination checks: C now extracts a previous
output only after parsing bounded owned bytes, matching their transaction ID
to the spending input and checking its output index. Amount/script substitution
therefore refuses; inclusion, unspentness, maturity and ownership are still
unqualified. Exact P2PKH/P2SH script decoding requires the caller's selected
network and refuses unknown or alternative templates without changing output.

All32 native ASan/UBSan/LSan tests pass, including both networks/kinds, every
opcode/hash-byte value, fixture truncations/byte mutations, wrong hashes/indexes,
independent projected output values and synthetic standard destinations.
Clang/GCC analysis and351 functions in61 files at complexity<=10 pass. The
expanded transaction/script/prevout fuzzer completed795,266 executions in91s
without a finding and final hashes match. The58 core and24 app JVM tests, both
ABI builds, lints, APK fixture isolation and architecture pass. No JNI/UI/device
route was changed. Evidence and preserved corpus are in prevout-* under the
48-hour scratch directory. Continue with bounded input/output totals, explicit
fee limits and an exact transaction-ID assessment; no change/ownership or
signing approval may be inferred from this data.

2026-09-13 bounded transaction assessment: C now matches one previous source
per input, requires explicit P2PKH/P2SH destinations for all input/output rows,
checks money totals and subtraction, and enforces an explicit absolute fee
ceiling. It publishes one owned1056-byte host result only after all checks and
the current transaction ID succeed. Network/policy remain explicit metadata;
the byte ID alone cannot bind them or provide consent. No change ownership,
inclusion, unspentness, signing or broadcast authority is added.

All33 native ASan/UBSan/LSan tests pass, including exact/excessive fees,
insufficient funds, combined-input money overflow, duplicate/distinct outpoints,
maximum counts/money, wrong source order/count and every funding-byte mutation
and truncation. Clang/GCC analysis and356 functions in62files at complexity<=10
pass. Dedicated assessment fuzzing completed2,862,743 executions in91s without
a finding; final source/binary hashes match. The58 core and24 app JVM tests,
both ABI builds, both lints, APK isolation and architecture pass. Logs, synthetic
seeds, corpus, hashes and size measurement are in assessment-* under the48-hour
scratch directory. Continue with independently checked change-address derivation
and immutable review/key ownership before original branch-specific signing.

2026-09-13 public change-address derivation: the C receiving wrapper retains
external chain0 and a new change wrapper selects internal chain1, sharing the
existing entropy/profile/blinding and deterministic secret-cleanup path. It
returns only a public address. No change index is reserved or persisted and no
JNI change, signing or ownership-approval route is added.

All34 native ASan/UBSan/LSan tests pass, including both branches' argument
bounds, blinding independence, allocation/context failures, exact invalid-child
refusal at every path level and early/middle/final provider failures. The
independent OpenSSL oracle passed96 comparisons across published seeds, both
networks/branches and indices through2^31-1 in1.93s. Clang/GCC analysis and358
functions in62files at complexity<=10 pass. The full-derivation fuzzer completed
716 executions in61s with no finding and matching final source/binary hashes.
Every nonempty in-bound iteration includes three complete derivations as well
as refusal cases.

The58 core and24 app JVM tests, both ABI builds, both lints, APK isolation and
architecture pass. The API35 public GCM/storage/receiving-address JNI fixture
passed one test in4.813s using its own new isolated directory; no real wallet
was opened or erased. Evidence/corpus/hashes are in change-* under the48-hour
scratch directory. Continue with reviewed standalone C BLAKE2b/sighash support
and immutable review/key ownership; do not link the sealed consensus core or
enable signing until the original branch/context and authorization are qualified.

2026-09-13 BLAKE2 pre-integration candidate blocked: Clang20 reported a possible
uninitialized parameter-block byte in the official pinned reference provider.
One text-diagnostic pass preserved the call path; runtime defect versus analyzer
limitation is unresolved. The strict script stopped before GCC or sanitizer
self-tests. No source was patched, suppressed, vendored or linked into wallet
builds. Exact source/license/KATs/logs/command/commit metadata are archived and
hashed; see [BLAKE2_REVIEW.md](BLAKE2_REVIEW.md). TLS evidence/quarantine is
untouched. A read-only fetch still shows origin/main a898cf779,35ahead/16behind;
no integration occurred. Move immediately to immutable transaction-review
lifetime work, without making either provider review the active investigation.

2026-09-13 immutable unsigned review lifetime: one caller-owned C owner now
retains canonical unsigned bytes, exact assessment/network/fee policy and a
nonreused positive ID. Preparation publishes only after complete validation.
Copied snapshots and wire buffers cannot mutate the retained draft. Stale IDs
cannot sample its clock; rollback and inclusive90s expiry clear it. Reads never
extend its deadline, and clear/cancel preserves issuance. This is public review
data with no consent, ownership, chain, JNI, signing or broadcast authority.

The optimized arm64 build caught a5344-byte frame after inlining preparation.
Separating preparation into its own C unit fixed it while retaining the4096-byte
warning limit. The initial failure log and both bounded fuzz corpora remain
preserved. Final35 native ASan/UBSan/LSan tests, Clang/GCC analysis and366
functions in64files at complexity<=10 pass. Final sequence fuzzing completed
152,143 executions in91s without a finding; source/binary hashes match.
Measured host owner3024bytes/snapshot1064bytes; no heap was added.

The58 core JVM tests pass; unchanged24 app JVM cases remain up-to-date and
passing. Both ABI builds, both lints, APK isolation and architecture pass. No
JNI/UI/device path changed, so no unchanged device suite was rerun. Evidence
is in review-* and review-final-* under the48-hour scratch directory. Continue
with exact outpoint/sequence/lock/expiry snapshot context before the thin JNI
projection; no finality or current-chain validity may be inferred from fields.

2026-09-13 exact review context: the same immutable snapshot now carries raw
uint32 lock/expiry and each input's display-order previous ID, output index and
sequence beside accounting. It derives all fields from the already owned
parsed transaction. High-bit fields, maximum accepted expiry, all eight rows,
indexes through15 and eight-to-two replacement cleanup have deterministic
coverage. No contextual finality, replacement, chain or ownership claim is added.

All35 native ASan/UBSan/LSan tests, Clang/GCC analysis and367 functions in64files
at complexity<=10 pass. Extended context/lifetime fuzzing completed127,332
executions in91s without a finding and with matching final source/binary hashes.
The58 core JVM cases, unchanged24 app JVM cases, both ABI builds, both lints,
APK isolation and architecture pass. No JNI/UI/device route changed. Host owner
is3352bytes and snapshot1392bytes; no heap was added and the4096-byte frame gate
still passes. Evidence/corpus is in review-context-* under the48-hour scratch
directory. Continue with the thin JNI owner and public projection, including
exception/cleanup, stale-ID, cancellation and empty-restart fixtures.

2026-09-13 unsigned review JNI/managed bridge: one process-wide C owner now
accepts bounded copied public drafts/previous transactions and returns exact
268-long maximum snapshots or copied unsigned bytes. One17472-byte input
allocation clears before free; all local refs are bounded/deleted. Java result
allocation happens after unlocking. Failed publication cancels only the original
ID, including a deterministic close/reopen interleaving at allocation.

The managed owner samples its clock under serialization, closes on read/clock/
decode failure, cancels construction failures and checks close results. Its
rows/lists are immutable public copies. Destinations expose kind/hash/network/
value; canonical address display and an actual review Activity remain next.
No signing, authentication, transport, endpoint or persisted review is enabled.

All36 native ASan/UBSan/LSan tests, Clang/GCC analysis and388 functions in66files
at complexity<=10 pass. JNI exception/sequence fuzzing completed21,502runs/91s;
after adding the publication interleaving regression, focused sanitizer tests
and a supplemental7,659runs/31s pass with final source/binary hashes matching.
Both original and supplemental corpora/logs remain preserved.

All64 core JVM and24 app JVM tests pass, including6 new real-JVM review cases
with-Xcheck:jni. Both ABI builds, both Android lints, architecture and APK
isolation pass. Isolation additionally rejects leaked review fixtures in an
app APK and a missing required transaction in the test APK. The API35 emulator
passed3 public JNI review cases in7.504s, covering both networks, immutable
copies, repeated replacements, rollback and inclusive expiry. This does not
qualify an Activity, hardware custody or original-node transaction acceptance.
Evidence is in review-jni-* under the48-hour scratch directory. Continue with
canonical P2PKH/P2SH destination encoding and foreground review presentation;
key/change ownership and branch-specific signing remain separate gates.

2026-09-13 canonical transparent destination encoding: C now encodes an explicit
network/kind/public hash for both P2PKH and P2SH using the existing prefixes and
bounded Base58Check encoder. The original P2PKH-only helper delegates while
preserving validation order and receiving-address bytes. No JNI entry or
ownership/signing capability is added in this slice.

Four original public address/script vectors were rechecked against the pinned
reference, with all short capacities, canaries, arbitrary hash values and bad
network/kind/argument refusals. All37 native ASan/UBSan/LSan tests, Clang/GCC
analysis and389 functions in66files at complexity<=10 pass. Dedicated address
encode/parse fuzzing completed111,149 executions in61s without a finding;
final source/binary hashes match. All64 core/24 app JVM cases, both ABI builds,
both lints, APK isolation and architecture pass. No unchanged device suite was
rerun. Evidence/corpus is in address-encode-* under the48-hour scratch directory.
Continue with the thin JNI/managed address factory and exact review destination
text, then foreground review presentation without signing authority.

2026-09-13 canonical review destination factory: a fixed21-byte public-record
JNI entry now validates network/kind before C encoding. The managed P2SH
factory owns its copied hash record, and review destinations derive canonical
P2PKH/P2SH addresses from bounded public fields. No ownership/change label,
signing action or review screen is introduced.

All38 native ASan/UBSan/LSan tests, Clang/GCC analysis and390 functions in66files
at complexity<=10 pass. The dedicated JNI record/exception fuzzer completed
212,176 executions in61s without a finding and final source/binary hashes match.
Tests cover every type byte, record sizes, network bounds, partial region reads,
allocation failures with/without pending exception, and output-region failure.
All66 core/24 app JVM cases, both ABI builds, lints, APK isolation and architecture
pass. The API35 emulator passed3 updated review cases in6.111s, including exact
destination scripts and selected networks. Original public address vectors
independently qualify both factories. Evidence is in address-jni-* under the
48-hour scratch directory. Continue with owned foreground review presentation,
one C-driven expiry wakeup, failure cleanup and restoration/recreation fixtures.

2026-09-13 foreground unsigned review delivery: an owned review presentation now
reuses the balance queue/timer engine. Every delivery samples its original C
owner; expiry wakeups carry only a redraw signal. Close releases owner/receiver
references, drops late callbacks and requires a newly prepared replacement.
Failure cleanup now clears the display even if timer cancellation or owner close
throws, preserving renderer/fatal and cleanup exceptions. Existing balance
renderer-failure tests now require clearing as well as propagation/cancellation.

All66 core and35 app JVM cases pass, including11 new review timing/replacement
and cleanup-failure cases. Both ABI APK builds, both lints, fixture isolation
and architecture pass. The API35 emulator passed10 existing balance presentation
and balance/history lifecycle cases in215.681s. No native source changed, so
unchanged sanitizer/fuzz campaigns were not repeated. Evidence is in
review-presentation-* under the48-hour scratch directory; the initial test
compile failure (shadowed loop variable) is preserved separately. The review
view and its device fixtures are the next independent slice; this checkpoint
has no review Activity, signing, custody or broadcast claim.

2026-09-13 unsigned review display: ReviewView now displays complete canonical
output/funding addresses, C-formatted totals/fee/limit, selected network and
unsigned/unverified funding labels. Raw draft/outpoint IDs and unsigned32
context remain fully inspectable without finality or ownership assertions.
Earlier text clears before formatting; bounded malformed data cannot leave a
partial or previous transaction visible. Hierarchy restoration, autofill and
content capture are excluded; there is no cached countdown or sending action.

All35 app JVM cases, both ABI builds, both lints, APK fixture isolation and
architecture pass. The API35 emulator passed6 new view cases in12.42s, covering
both networks, full-width fields, failure clearing, forged old TextView state,
and inclusive C expiry through the actual main-queue timer. Native code and
core behavior are unchanged; existing66 core JVM and38 native sanitizer tests
remain the baseline, with no unchanged fuzz campaign repeated. Evidence is in
review-view-* under the48-hour scratch directory. The initial byte-count
localization lint finding was corrected through unit-label wording, without a
suppression; its log is preserved. Continue with actual foreground/recreation
Activity acceptance and process-relaunch fixtures.

2026-09-13 review Activity lifecycle: a nonexported debug-only FLAG_SECURE host
now receives explicitly prepared public review owners from instrumentation.
Background/close/recreation clear the old owner and view; an owner arriving
after foreground loss is immediately cancelled. The single native slot remains
unchanged. The screen has one obscured-touch-filtered close action and no
wallet/Keystore, intent data, persisted ID, fixture assets or automatic replay.

All35 app JVM cases, both ABI builds, lints, fixture isolation and architecture
pass. Independent per-host export/missing-export/missing/release mutations
qualify both debug hosts; a missing export cannot borrow a following component's
attribute. The API35 emulator passed4 new review lifecycle cases in146.594s,
including repeated recreation, late prepared-owner refusal, queued close and
close/new-Activity behavior. Native/C-core behavior is unchanged; no unchanged
sanitizer/fuzz campaign was repeated. Evidence is in review-activity-* under the
48-hour scratch directory. Continue immediately with public review process
termination/relaunch acceptance; signing and authenticated funding remain gated.

2026-09-13 review process-restart acceptance: the existing opt-in emulator
controller now has a review profile, retaining its exact-PID/readiness/profile
checks and new-report-directory rule. A live public unsigned draft was verified
before terminating PID24937. New PID24997 started with an empty view and free
native review slot; explicitly preparing another draft did not auto-display it.
The relaunch case passed in24.129s. The intentionally killed preparation reports
process loss, as expected; this is not a native parser or sanitizer failure.

Invalid profile and physical-device arguments refuse before adb or directory
creation. Both ABI artifacts, both lints, fixture isolation and architecture
pass; the unchanged35 app/66 core JVM and38 native safety cases remain the prior
baseline. No unchanged unit/sanitizer/fuzz suite was repeated. Evidence is in
review-process-* and review-process-acceptance under the48-hour scratch directory.
Continue with a bounded C unsigned-draft constructor using existing canonical
parsing/assessment, without implying spendability, change ownership or signing.

2026-09-13 bounded C unsigned-draft constructor: fixed public funding selections
and explicit network/destination/value/context/fee policy now produce an owned
unsigned transaction only after canonical source/index checks, exact script
construction and the existing full assessment. Input/output order is preserved;
no coin selection, change reservation, authenticated funding or signing is added.
Every refusal leaves the full caller result unchanged. Separate bounded C
translation units preserve the4096-byte frame gate without allocation or waiver.

All39 native ASan/UBSan/LSan tests, Clang/GCC analysis and397 functions in68files
at complexity<=10 pass. Structured/raw-previous-wire fuzzing completed195,757
executions in91s without a finding; final source/binary hashes match. Exact
fixture bytes and the independently qualified OpenSSL draft ID match on both
networks. Host request872 and transaction2200bytes; optimized Clang20 frames
measure2264/2216/1192bytes. Both Android ABIs,66 core/35 app JVM gates, lints,
fixture isolation and architecture pass. No unchanged device route was rerun.
Evidence/corpus is in draft-* under the48-hour scratch directory. Continue with
a thin bounded constructor/review adapter, retaining all source ownership,
error-publication and foreground cancellation requirements. No JNI constructor
or current-chain/funding/signing claim is included in this checkpoint.

2026-09-13 draft JNI and managed preparation: the stateless C adapter now
constructs from bounded previous-byte/address arrays and exact signed-long
metadata. Numeric widths and canonical selected-network addresses validate in
C before construction. One fixed16272-byte source allocation clears/frees
before Java publication, including partial VM reads and allocation failures.
UnsignedReview.prepare keeps the same private source copies through construction
and opening, clearing temporary source/draft bytes afterward. BUSY or malformed
preparation cannot cancel the existing review. No key, change, consent or send
authority is added.

All40 native ASan/UBSan/LSan cases, Clang/GCC analysis and412 functions in71files
at complexity<=10 pass. Fake VM fault injection checks reference/heap cleanup,
partial reads, signed extremes, malformed shapes and maximum requests. The
bounded JNI fuzzer completed40,621 executions in91s without a finding; final
source/binary hashes match. Optimized Clang20 host JNI frame2328bytes preserves
the4096-byte gate. All72 core/35 app JVM cases, both ABI builds, both lints,
fixture isolation and architecture pass. The API35 emulator passed9 review
and Activity lifecycle cases in143.352s, including the same privately copied
source handoff and construction-to-display recreation/background/close path.
Evidence/corpus is in draft-jni-* under the48-hour scratch directory. The
initial12 focused JVM cases preceded an additional clock-callback source-mutation
regression in the full run. Continue with authenticated key/change ownership
and durable index recovery; quarantined providers remain disabled and preserved.

2026-09-13 secret JNI exception refusal: custody inspection found that shared
byte helpers and phrase readers could start VM array operations with an exception
already pending. A deterministic fake VM rejected that forbidden call before
the fix. Early guards now preserve the original exception and return before
array access/allocation; entropy creation also avoids RNG work on an unusable
VM context. Existing partial-read/publication cleanup remains intact.

All41 native ASan/UBSan/LSan tests, Clang/GCC analysis and412 functions in71files
at complexity<=10 pass. The new fault fixture covers all five key JNI entries,
NULL/pending VM state, each read/publication failure ordinal, RNG failure,
malformed lengths/text and observed zeroization of live native spans. Bounded
key JNI fuzzing completed44,056 executions in91s without a finding; final
source/binary hashes match. All72 core/35 app JVM gates, both ABI builds, both
lints, fixture isolation and architecture pass. The API35 emulator passed2
new mnemonic/RNG cases and the existing public GCM/storage case in5.669s.

Evidence is in jni-key-* under the48-hour scratch directory, including the
failing baseline and four exact source hashes verified against its preserved
source snapshot. That initial fixture stopped before an unqualified address
literal; the final passing fixture compares the adapter against the already
qualified C derivation instead. No new independent address oracle is claimed.
The baseline is an injected JNI-contract failure, not a production crash or
sanitizer memory finding. No exception is suppressed, no hardware custody gate
changes, and VM/GC copies are not claimed physically erased. Resume authenticated
key/change ownership and durable index recovery after this focused fix.

2026-09-13 recovered-wallet internal address binding: a small C wrapper now
requires the existing v1 header/recovered-entropy match before deriving the
explicit internal-chain index on that same network/account. Two independent
32-byte blinding values serve the separate wallet-check/change derivations.
The wrapper returns only35 public address bytes and publishes nothing on any
failure. It adds no private-key output, mutable record field, JNI entry, index
reservation, output classification or transaction authorization.

All42 native ASan/UBSan/LSan cases, Clang/GCC analysis and414 functions in72files
at complexity<=10 pass. Deterministic tests cover both networks/all five entropy
widths/index0,1,2^31-1; header truncations/bit edits; wrong-wallet entropy;
length/NULL/capacity/index bounds; and full output canaries. Context/allocation
faults and invalid children at every one of the ten derivation steps leave no
partial output or owned context. Bounded full-binding/raw-header fuzz completed
869 executions in91s without a finding, with final source/binary hashes matching.
Each fuzz input completes valid wallet and change derivations before mutations.

Both Android ABIs,72 core/35 app JVM gates, both lints, fixture isolation and
architecture pass. No unchanged device route was rerun for this C-only API.
Evidence is in wallet-change-* under the48-hour scratch directory. Platform GCM
and qualified per-use hardware remain prerequisites for a real recovered-wallet
claim. Continue with durable index reservation/restart recovery and explicit
transaction authorization binding; seed-restoration discovery remains required.

2026-09-13 test-observer lifetime: the JNI key fault fixture now clears saved
pointers as soon as it observes their live buffers zeroed. Subsequent cleanup
checks keep the recorded result without comparing a pointer after its stack
object's lifetime ends. The focused JNI key ASan/UBSan/LSan fixture passes;
production source is unchanged. Evidence/source/binary hashes are in
jni-key-tracker-* under the48-hour scratch directory. This small test-only
checkpoint remains separate from the in-progress authenticated index codec.

2026-09-13 authenticated change-counter codec: an exact80-byte internal-chain
record now binds its uint32 next index to recovered wallet identity through
private HKDF-SHA512/HMAC keys. All key material clears on success/failure;
only the public record or verified counter publishes. The codec performs no
reservation, increment, storage, freshness check or transaction approval.
Old authenticated content still verifies, explicitly tested and documented;
the next storage slice must enforce durable monotonic consumption.

All44 native ASan/UBSan/LSan tests, Clang/GCC analysis and420 functions in74files
at complexity<=10 pass. Independent OpenSSL3.0.13 HKDF/HMAC matched40 complete
records across network/entropy/counter boundaries. Regular tests cover640 record
bit edits, truncations, wrong wallet/network, header mutations and canaries.
Faults at extract/expand/tag generation verify live key cleanup and unchanged
caller results. Bounded full-codec/raw-record fuzzing completed966 executions
in91s without a finding; final source/binary hashes match.

Clang20 host and NDK ARM64 -O2 assembly inspect every tag byte before equality;
encode/decode frames are152/168 and176/192bytes respectively. Both Android
ABIs,72 core/35 app JVM gates, both lints, fixture isolation and architecture
pass. This C-only codec adds no JNI/device route; unchanged device cases were
not rerun. Evidence is in change-state-* under the48-hour scratch directory.
Continue immediately with bounded durable reservation and explicit incomplete-
write recovery; missing/corrupt state must never silently restart at zero.

2026-09-13 durable change IO: fresh paired creation now persists authenticated
initial-state bytes before any pending wallet write. Both creation routes
refuse orphan state. Existing wallets missing state never initialize at0.
Bounded observation and compare-and-append match the exact committed wallet
and snapshot under the existing nonblocking lock, using the same state
descriptor for read/write. Complete uncertain appends consume their prior
index; partial data remains intact and blocks normal reservation. The5MiB cap
permits65535 reservations without compaction or rollover.

All47 native ASan/UBSan/LSan tests, Clang/GCC analysis and433 functions in76files
at complexity<=10 pass. Fault fixtures cover short/EINTR/failed IO, partial
writes, each flush/close boundary, unchanged failed observations and descriptor
counts. Child processes reached26 interruption boundaries; exactly one of12
competing appenders succeeded. These are process/IO tests, not power-loss or
malicious-filesystem-rollback proof. Bounded raw/structured file fuzzing completed
124907 executions in91s without a finding; source/binary hashes matched.

Both Android ABIs,72 core/35 app JVM gates, both lints, fixture isolation and
architecture pass. Evidence/source/binary hashes and corpus are preserved in
change-storage-* under the48-hour scratch directory. No new JNI/device route
was introduced, so unchanged device cases were not repeated. The public-data
IO adapter requires caller GCM/recovered-wallet/state authentication; it grants
no index/address publication itself. Continue with an authenticating reservation
wrapper, explicit append-only repair, migration and gap-aware seed discovery.

2026-09-13 authenticated reservation: C now owns a bounded private ciphertext
copy across recovered-wallet/MAC checks, internal-address derivation and durable
append. Fresh creation encodes state0 internally. Reservation takes no caller-
selected index or state, uses fresh OS blinding for each cryptographic step,
clears it on every exit, and publishes index/network/address only after all IO
cleanup succeeds. Wrong entropy, bad MAC, wrong position, missing/partial state,
stale observation and exhaustion refuse without changing the caller's result.

All49 native ASan/UBSan/LSan tests, Clang/GCC analysis and439 functions in77files
at complexity<=10 pass. Both networks/all entropy widths, final index65534 and
exhaustion have deterministic coverage. Source-only faults check live blinding
cleanup and partial RNG/crypto outputs. Every close/flush across observe/append,
partial write, ciphertext mutation after private copying and a competing append
exercise unchanged failure results and conservative index consumption.

Initial fuzzing completed2930 executions in91s but grew only to13-byte inputs.
A reusable host seeder added complete authenticated, bad-MAC, misplaced and
partial-successor public records. A focused follow-up completed4234 executions
in61s from86..166-byte seeds without a finding. Both corpora, logs and matching
source/binary hashes are preserved in change-reservation-* under the48-hour
scratch directory. The seeder is fuzz-only; production C did not change for
that follow-up. Both Android ABIs,72 core/35 app JVM gates, both lints, fixture
isolation and architecture pass; unchanged app JVM cases were UP-TO-DATE.
No new JNI/device path exists, so device tests were not repeated.

GCM/per-use hardware authentication remains a platform prerequisite, and public
reservation metadata is not transaction consent or a change-output receipt.
Continue immediately with explicit append-only repair: preserve bytes, burn
uncertain gaps, refuse missing files and detect authenticated counters at wrong
positions. Migration, seed discovery and exact review binding remain required.

2026-09-13 append-only repair IO: a checked public-data plan pads an incomplete
slot with at most80 zero bytes, then appends its authenticated next-position
record. At most160 bytes are added within the existing cap. It never changes
an earlier byte or creates missing state. Exact wallet/snapshot comparison and
the existing descriptor/lock/durability rails are shared with normal append.
No normal-reservation fallback or authenticating recovery caller is added.

All52 native ASan/UBSan/LSan tests, Clang/GCC analysis and445 functions in78files
at complexity<=10 pass. All prefix lengths0..159, capacity boundaries, missing/
stale/wrong-wallet refusal, padding/replacement partial failures, repeated repair
and descriptor cleanup have deterministic coverage. Twelve reached process-
interruption boundaries preserve every original and subsequently appended byte.
The older storage/reservation fault suites also pass after extending the test
partial-write hook to a selected ordinal. Bounded structured/raw repair fuzzing
completed118566 executions in91s without a finding; source/binary hashes matched.

Both Android ABIs,72 core/35 app JVM gates, both lints, fixture isolation and
architecture pass; unchanged app JVM cases were UP-TO-DATE. No new JNI/device
path exists. Evidence and full public seed copies/corpus remain in change-repair-*
under the48-hour scratch directory. No power-loss or malicious-rollback proof
is claimed.

Continue with authenticated recovery evidence before exposing repair. File
length alone cannot establish a consumed-index bound after data loss. A
committed wallet with empty/short initial state must not be reset through this
primitive. Require a verified predecessor, supported interrupted-record prefix
and consistent positions; preserve/refuse ambiguous loss or unsupported formats
pending independent discovery. The next slice needs a bounded predecessor probe
and an authenticating recovery caller. Signing and both quarantines stay disabled.

2026-09-13 recovery predecessor probe: C now returns the current state tail and
immediately preceding complete record as owned public metadata, reading at
most160 state bytes on the same descriptor under the existing wallet/lock rules.
Files<=80 report no predecessor with zero bytes. A bounded shared pread helper
checks range arithmetic, and metadata/size are rechecked before publication.
Normal observation remains one-tail-only; probing does not authenticate or repair.

All54 native ASan/UBSan/LSan tests, Clang/GCC analysis and448 functions in78files
at complexity<=10 pass. Tests cover321 exact file lengths, actual authenticated
predecessors/partial successors, cap offsets, both reads, all stat/close faults,
descriptor counts and a size change before publication. Bounded file/argument
fuzzing completed79726 executions in61s without a finding; source/binary hashes
matched. Both Android ABIs,72 core/35 app JVM gates, both lints, fixture isolation
and architecture pass. No JNI/device route changed. Evidence remains in
change-probe-* under the48-hour scratch directory.

Continue with authenticated recovery for supported interrupted records backed
by a verified predecessor. Missing/invalid predecessors, unsupported/too-short
prefixes, misplaced authenticated counters and ambiguous loss remain refused;
the public probe is evidence to verify, never repair authority by itself.

2026-09-13 authenticated suffix recovery: the C caller now requires a verified
immediate predecessor at its exact position and all 16 supported current-prefix
bytes before appending a successor. Healthy heads cause no write; authenticated
misplaced heads, missing/short initial state, unknown prefixes, exhausted capacity
and ambiguous loss refuse. Recovery returns no address or authorization, never
runs as a reservation fallback and preserves every existing byte.

All 57 native ASan/UBSan/LSan tests pass in 42.89s, including 18 reached recovery
process boundaries, all original lengths 0..159, both networks/all entropy
widths, capacity and crypto/IO fault cases. Clang/GCC analysis and all 456
production functions in 80 files at complexity <=10 pass. Shared private custody
helpers retain reservation's original blinding/cleanup sequence and its suites
remain green. The authenticated caller explicitly refuses a partial replacement
whose immediate predecessor is invalid; this limitation is preserved and tested.

Bounded recovery fuzzing completed 5,302 executions in 91s without a finding,
starting with complete authenticated, supported partial and damaged-MAC seeds.
This campaign explored inputs through 166 bytes; deterministic tests separately
cover larger/cap-boundary states. Source/binary hashes matched. Both Android ABIs,
72 core/35 app JVM gates, both lints, fixture isolation and architecture pass;
unchanged app JVM tests were UP-TO-DATE. No JNI/device route changed. Full evidence
and corpora remain in change-recovery-* under the 48-hour scratch directory.

Continue with fresh Android paired wallet/change-state creation, then exact
review/change/authorization binding. Restoration cannot initialize a historical
change counter from zero without discovery evidence. Existing records remain
preserved, and hardware custody, TLS and BLAKE2 blockers stay unchanged.

2026-09-13 continuation: finished the pending Android fresh paired-creation
adapter on `agent/android-wallet-20260911`. CREATE persists authenticated change
state before the encrypted wallet; RESTORE retains wallet-only persistence and
UNLOCK cannot enter creation. JNI copies bounded inputs and clears its complete
native entropy buffer on success, refusal and partial VM reads. Existing wallet,
pending and orphan records cannot be overwritten or reset.

Resumption found the previous session stopped after a fuzz assertion caused by
truncating its trusted temporary-directory path. The original unsafe harness,
crash input and public artifacts remain preserved; it was not rerun. The fixed
harness passes a registered regression covering the reproducer, all 256 path
selectors, a real paired write and six VM failure ordinals. A new ASan/UBSan/LSan
campaign completed 18,490 executions in 121 seconds without a finding, and final
source/binary hashes matched. The original 58 native tests and new regression
pass; Clang/GCC analysis and all 457 production functions at complexity <=10 pass.
The sandbox initially prevented LeakSanitizer process inspection; the full suite
passed with that access restored and leak detection still enabled.

Both Android ABIs, 113 JVM tests, debug/release lint, APK fixture isolation and
architecture pass. Nine API-35 emulator tests pass for public GCM paired/wallet-only
storage, native key handling, secret views and authentication timing (13.129s).
These do not qualify physical hardware custody. Repository lint remains red on
existing injected `.agents`/`.codex` root entries, its flag-registry selftest and
16 over-complex wallet test functions. The latter are the next defensive cleanup;
no threshold or baseline was weakened. Evidence is under the ignored
`.cache/android-wallet/resume-20260913` directory.

2026-09-13 fixture continuation: finished the inherited C test/fuzzer refactor.
All 16 complexity violations are removed by extracting bounded helpers while
preserving the existing assertions and cleanup. The wallet safety command now
requires a nonempty fixture census at the existing <=15 cap, independently of
the unchanged production <=10 cap. It observes 761 fixture functions in 101
files and 457 production functions in 80 files; no baseline was loosened.

All 59 ASan/UBSan/LSan tests pass in 44.10 seconds, with authored Clang/GCC and
enabled-provider Clang analysis. Eleven changed fuzz harnesses complete
1,391,788 executions over 231 aggregate seconds without a finding, using copied
public corpora, bounded inputs/RSS/time and leak detection. Each campaign's
binary hash rechecks. Logs, corpora and final source hashes are preserved under
`.cache/android-wallet/continue-20260913`. A reused CMake cache discarded its
requested fuzz settings during regeneration; a new invocation-owned build
directory was configured and its sanitizer/fuzz settings verified before use.

Repository lint-fast now passes 30/32 gates. The remaining pre-existing failures
are the injected `.agents`/`.codex` root directories and the flag-registry
selftest's empty tracked-file scan. Neither was hidden or weakened. Work stays
on the existing Android branch, with no merge, push, custody or consensus change.
Continue with QR callback deadlines: Handler cleanup alone does not reject a
reply whose delivery is delayed beyond its allowed elapsed-time window.

2026-09-13 QR deadline continuation: the isolated Binder adapter now checks
elapsed time at connection/reply arrival and queued delivery, including after
public request parsing. Existing cleanup timers, cancellation, array clearing
and network validation remain. Bounds are unchanged at 15 seconds for ready
and 5 seconds for a reply; rollback/negative time refuses, and subtraction
avoids overflow at the signed clock limit.

Six API35 tests pass in 61.466 seconds, including new real-Binder queued
expiry/rollback and valid-boundary fixtures. Removing just the delivery checks
in a temporary build makes the regression fail in 20.344 seconds because the
expired reply is delivered. Fixed source is restored. Debug/test and minified
release builds, JVM tests, both lints and fixture isolation pass. Evidence is
under `.cache/android-wallet/continue-20260913`. The initial instrumentation
attempt overlapped completion of test-APK installation and reported process
death; the successful run started after installation completed. No wallet data
was cleared.

Continue immediately with the scanner JNI boundary: unlike the shared array
helpers, direct camera/scanner entries still accessed the VM before checking
for an already-pending exception. Add a registered fake-VM fault/cleanup fixture
and preserve the independent real-VM and emulator acceptance.

2026-09-14 scanner JNI continuation: all three camera/scan entries now refuse
NULL environments and pending exceptions before ordinary VM access. The new
registered fake-VM fixture fails on the preceding implementation, then passes
with the guards. It covers all four packing/five decoding VM failure ordinals,
partial reads, allocation refusal, exact public QR results, untouched inputs,
output canaries and full clearing of invocation-owned allocations before free.

All 60 native ASan/UBSan/LSan tests pass in 44.23 seconds. Authored Clang/GCC
analysis and the unchanged production/test complexity caps pass. Bounded camera
and QR campaigns complete 1,675 and 1,604 executions respectively over 61 seconds
each, using preserved public corpora with leak detection; binary hashes recheck.
Both Android ABIs, JVM tests, debug/release lint and fixture isolation pass.
Eight API35 QR/Binder tests pass in 73.337 seconds on the final scanner APK.
Architecture passes; repository lint-fast retains only its two existing failures.
Evidence remains under `.cache/android-wallet/continue-20260913`.

The broader JNI audit found the same pending-exception gap in the oldest amount
adapter's duplicated VM reads/publication. Continue by reusing the common helpers
and preserving its signed status mapping, with a failing-before/passing-after
fake-VM regression. Custody, signing, transport and BLAKE2 boundaries are unchanged.

2026-09-14 amount/snapshot JNI continuation: amount parsing and formatting now
reuse the shared checked byte-array helpers, removing duplicate publication
code and refusing existing exceptions. Negative/oversized length still returns
INVALID_ENCODING; numeric overflow still returns OUT_OF_RANGE. New fake-VM tests
preserve exact zero/one-zatoshi/maximum values, signed formatting, malformed
lengths, every read/publication exception and allocation failure without an
exception. The pending-call regression fails on the prior implementation.

Both sync snapshot entries now check pending exceptions before native owner
lookup or VM allocation. A regression fails against the previous implementation;
after the fix, refused reads at expiry cannot advance or expire either history
mode's owner. The same refusal is injected at INT64_MAX in every JNI sync fuzz
iteration. A bounded public-corpus campaign completes 11,592 executions in 61
seconds without a finding, with source/binary hashes rechecked.

All 61 native ASan/UBSan/LSan tests pass in 44.13 seconds, plus Clang/GCC analysis
and unchanged complexity caps. Both Android ABIs, JVM tests, debug/release lint,
fixture isolation and architecture pass. Seven API35 amount/balance/history
tests pass in 2.688 seconds. The final scanner's additional actual-camera test
passes three foreground/background capture cycles in 136.905 seconds, checking
frame clearing, worker shutdown and explicit restart. Evidence is preserved in
`.cache/android-wallet/continue-20260913`. Repository lint-fast still has only
the two previously recorded failures; no threshold or guard was weakened.

Continue with camera-to-review interoperability. The existing emulator's back
camera is configured as `emulated`; its generated scene provides real camera
frames but no public QR target. A separate disposable virtual-scene fixture can
qualify that path without touching existing wallet or emulator data. Physical
camera/device qualification remains a separate gate.

2026-09-14 fixture-gate diagnosis: the repository flag self-test assumes a
chmod-000 file cannot be read. Root can still read it, so this session fails
that refusal assertion. The unchanged binary's self-test passes under an
unprivileged UID in an isolated temporary directory, including its expected
empty-scan and unreadable-file refusals. No assertion was skipped or weakened.
Running the tracked-tree scan separately exposed four wallet fixture-script
variables missing from the authoritative registry. Registering them as
env_test, with checked first-use pointers, makes that complete scan pass.
This metadata change grants no runtime feature or wallet authority. Evidence
is under `.cache/android-wallet/camera-scene-20260914/lint-flag-*.log`.
The original lint-fast failures remain visible for root execution and the
preserved injected `.agents`/`.codex` root directories.

2026-09-14 Android native layout: inspected release ELF headers showed 4096-byte
LOAD alignment on both arm64-v8a and x86_64. Enable the pinned NDK27 flexible
page-size option, which sets 16384-byte maximum page alignment and removes the
Bionic compile-time PAGE_SIZE macro. No authored native source uses page-size
macros or mmap/mprotect/msync/munmap; this change alters build layout, not
wallet buffers, allocation arithmetic, object ownership or secret cleanup.

The new `checkNativeAlignment` task is part of Android `check`. It verifies the
actual debug/release APKs with SDK zipalign, requires exactly the two expected
native libraries, and checks every ELF LOAD segment's power-of-two alignment
and matching file/virtual-address low bits. Tool output must parse as bounded
hex before shell arithmetic; no input is evaluated as a command. The check
rejects preserved pre-fix APKs, an APK realigned to only 4 KiB and an aligned
archive containing a third native library. No deployed APK was mutated for
these negative fixtures.

Both ABI builds, JVM tests, Android debug/release lint, fixture isolation,
native alignment and architecture pass. Eleven API35 public-fixture JNI,
storage, amount, QR and isolated-Binder tests pass in 26.106 seconds on the
rebuilt app. Repository lint-fast retains the two diagnosed environment
failures. Evidence is under `.cache/android-wallet/page-alignment-20260914`;
the gate also saves exact APK hashes and ELF reports. This establishes build
layout plus runtime on the existing 4 KiB emulator, not a 16 KiB device test,
hardware custody, physical-camera acceptance or consensus qualification.

2026-09-14 native page-size runtime continuation: a fresh official API35
Google APIs 16 KiB x86_64 emulator reports PAGE_SIZE=16384. The preserved
pre-alignment APK installs but its public mnemonic test fails loading the JNI
library with an UnsatisfiedLinkError. The aligned APK passes all eleven public
key, storage/GCM, amount-presentation, QR and isolated-Binder tests in 49.146
seconds. The test APK, fixtures and assertions are unchanged between the
before/fixed runs. Exact APK hashes and device fingerprint are retained under
`.cache/android-wallet/page-alignment-20260914/device16k-*`.

The first fixed run passed both key tests before Android killed instrumentation
for a startup ANR; exit-info records reason=6, not a native crash. On this
disposable profile only, disabling repeatedly failing Google services/search
and Bluetooth through normal package-manager controls allowed the full rerun.
The attempt to disable page-size compatibility through shell properties was
refused; no property bypass was used or claimed. The old-library failure and
fixed-library success are observed runtime evidence, not a claim about a forced
compatibility setting. This qualifies x86_64 emulator JNI behavior at 16 KiB;
arm64 hardware, hardware-authenticated custody and physical optics remain open.

2026-09-14 complete emulator camera-to-review path: the host-only
`seed_camera_scene` reuses the pinned QR encoder and emits a fixed 640x480 PNG
using one checked allocation and a bounded stored-DEFLATE writer. Exclusive
creation refuses existing files; all writes/close are checked. Standalone
Clang/GCC analysis, ASan/UBSan/LSan, exact repeated output and forced short-write
refusal pass. The explicit C hazard review is recorded before commit.

The final opt-in CameraRequestInstrumentedTest passes in 88.517 seconds on a
fresh AOSP API35 x86_64 profile. Two actual Camera2 scans reach the exact public
address, 1.25 amount and CameraFixture label through the isolated decoder.
Preview pixels and the camera worker clear on review; Activity recreation
discards the request and requires the second explicit scan. The imagefile
backend rejects PPM and rotates/crops landscape PNG input; captured public
diagnostics established a safe target position. No decoder threshold, deadline
or production camera behavior changed to accommodate the fixture. Temporary
capture code and device files were removed before final acceptance.

All 61 native sanitizer tests pass in 44.72 seconds. Strict analysis, both ABI
builds, JVM tests, debug/release lint, fixture isolation, native APK alignment
and architecture pass; the two diagnosed repository lint environment failures
remain visible. Scene-fuzz evidence includes 4,335 executions in 121 seconds
with no finding; it used the initial public scene pixels, not the later
emulator framing adjustment. Evidence is under
`.cache/android-wallet/camera-scene-20260914`.

Continue with the actual permission dialog and minified runtime. A locally
signed minified APK already launches on the 16 KiB emulator and reaches its
public scanner, but a real permission denial returns to the generic chooser
message instead of its denial explanation. Reproduce that Android callback
ordering in an opt-in UI regression and preserve the existing camera gates.

2026-09-14 actual permission-denial continuation: Android can deliver denial
while the scanner is paused, then resume it. Previously that resume replaced
the result with the generic chooser message. An in-memory public denial flag
now preserves the explanation until a new scan attempt. Permission checks,
capture continuation, decoded-request clearing and saved-state contents are
unchanged.

The standalone `scanner-ui-tests` APK supplies its own pinned Kotlin/JUnit
runtime and uses only Android framework APIs and public view identifiers.
Reusing the debug instrumentation APK against R8 output failed because its
shared Kotlin classes had been removed from the target; no production keep
rule was added. The final fixture manifest is test-only, targets only the dev
package, and has no activity, service, provider or requested permission. It
refuses an existing wallet directory and opens only the private scanner.

The exact same fixture APK fails on the previous minified app in 62.253 seconds
after Android closes the denial dialog and the scanner regains focus. With the
fix, it passes in 22.042 seconds, observing the denial explanation, selected
mainnet, denied permission, and absence of preview, request, camera worker and
wallet directory. The debug scanner's network/recreation/background test also
passes in 57.579 seconds. Local development signing preserves all twenty entries
of the normal unsigned release APK, checked byte-for-byte; no test-driven R8
mapping or app dependency was introduced.

Both ABI builds, JVM tests, debug/release lint, standalone fixture checks,
fixture isolation, native alignment and architecture pass. Earlier attempts
encountered stacked system ANR dialogs and an unconsumed permission tap. Those
failures remain in the evidence; normal Wait actions cleared the system dialogs,
and the unchanged fixed-app retry passed. The fixture explicitly distinguishes
an undismissed permission dialog from a missing app explanation. Evidence and
exact APK hashes are under `.cache/android-wallet/camera-scene-20260914`.

Continue with reliable touch timing and the minified permission-grant to exact
camera-review journey. Hardware custody, physical optics, network integration,
TLS qualification and sending remain separate unfinished gates.

2026-09-14 minified camera continuation: the standalone fixture now optionally
continues after denial with a fresh permission request, actual foreground grant,
Camera2 capture and exact public review. It observes the fixed address, 1.25
amount, CameraFixture label, review notice and Scan again control through the
Android view tree, then requires no preview/camera worker and closes the scanner.
Every run still refuses a wallet directory. Touches now use the platform
UIAutomator's 100 ms press duration after UI idleness and fresh bounds; Android
touch filtering and production scanner deadlines are unchanged.

The normal minified APK passes the whole journey in 33.958 seconds and again in
32.714 seconds after resetting only the disposable profile's permission state.
The exact same fixture APK, signed target APK and PNG hashes recheck after both
runs. The normal release's twenty entries remain byte-identical after local
development signing; no keep rule, test mapping or production dependency was
added. Standalone debug/release compilation and strict lint pass. There is no
production C change in this slice. Evidence is under
`.cache/android-wallet/camera-scene-20260914/minified-*`.

Continue with real background process death/relaunch while the public review
is present, then explicit new capture in the replacement process. This will
test Android's process boundary separately from the existing Activity recreation
fixtures. Physical camera and hardware-authenticated custody remain unqualified.

2026-09-14 actual background process-death continuation: the same minified APK
passes normal Android task restoration after an explicit background kill. The
scanner first changes its launch intent's testnet selection to mainnet and
displays the public camera request. Android saves the stopped Activity state;
`am kill` removes PID 4854 with exit-info's background-kill reason while task 32
and its scanner Activity record remain. The normal launcher restores that
record in PID 5119 with mainnet selected, no request or preview, and Start camera
present. The camera service has no active client before the new action.

A fresh Start camera in the replacement process reaches the exact public
address, 1.25 amount and CameraFixture label. The service records that process's
connect/disconnect and is closed again at review. The signed target, standalone
fixture and PNG hashes still match. Evidence is retained in
`.cache/android-wallet/camera-scene-20260914/process-*`. The normal welcome
screen created an empty storage directory on this previously empty disposable
profile; no create, restore or unlock action ran, and the standalone fixture's
strict no-directory guard was not changed or bypassed. This is manual minified
OS task-restoration evidence, separate from debug Activity recreation fixtures.

Continue with cancellation at the real camera-open callback boundary, bounded
ownership under overlap, and recovery to a fresh capture. Physical-device and
hardware-authenticated custody acceptance remain open.

2026-09-14 pending camera-open delivery continuation: the new opt-in debug
fixture uses a real Camera2 request and a bounded worker-queue gate. It observes
`opening` on the camera worker, cancels on main, and orders the existing release
operation before the terminal callback using test-only reflection. The pending
owner/resources remain held, a competing capture is refused with one worker,
the real late callback closes without pixels, and a fresh owner receives an
actual frame before closing. No production hook, driver mock, timeout or
ownership rule changes.

Acceptance passes in 8.027 seconds on the existing Google API35 emulator and
4.103 seconds on the disposable AOSP API35 profile. A separately archived mutant
removing only the pending-open release guard fails the same test APK in 1.226
seconds at the intended ownership assertion. The first mutant build lacked the
pinned BIP39 provider in its selective archive; adding the unchanged provider
completed the build. No production source was mutated in this checkout. The
normal minified APK was restored on the disposable profile after both runs.
Strict lint, final fixture isolation, native APK alignment and architecture
pass. This slice changes test code only; there is no production C change.
Evidence is under `.cache/android-wallet/camera-scene-20260914/open-cancel-*`.

Continue with the oldest supported API30 runtime on a newly created, isolated
Android 11 emulator profile. Its official system image and image-backed camera
are prepared; boot and runtime acceptance are not yet complete. Hardware
custody, physical optics and nonresponding-driver behavior remain unqualified.

2026-09-14 oldest-API and compact chooser continuation: a fresh AOSP API30
x86_64 image passes the normal minified permission-denial/retry/grant/Camera2
journey in 29.961 seconds, all eleven selected public native/JNI tests in 8.185
seconds, and unauthenticated Keystore refusal plus camera-open cancellation in
2.110 seconds. The native tests include both authenticated GCM record/store
paths, deadline boundaries, QR rendering and isolated Binder identity. They
create only invocation-owned fixture records and aliases, not a wallet session.

A 320x240 dp chooser with actual system-bar insets exposed an unreachable
network selector. Its title, explanation, network and actions now share a
scrollable body with saved view state disabled. The final exact same layout
test APK fails before this change and passes after it in 9.597 seconds, checking
whole-control reachability and the selected network delivered to Start. Initial
fixture failures came from Android mutating a supplied rectangle and a radio
button's click-return semantics; both are corrected without changing app code
for those false failures. The no-insets run passed, so the final fixture applies
the actual insets and checks the padded viewport explicitly.

The compact chooser and Activity network/recreation tests also pass together
on the existing API35 emulator in 58.670 seconds. The final minified APK passes
the full API30 public camera journey in 25.362 seconds. Its twenty unsigned
release entries remain identical after local development signing; final APK,
fixture and PNG hashes recheck. The newer per-permission flag reset is absent
on API30, so only this newly created disposable profile used the supported
runtime-permission reset. The no-wallet-directory guard remains enforced; no
app data, key, directory or assertion was removed for the reset. First boot
took 417 seconds and its System UI ANR required a normal Wait action before
acceptance. Evidence is in `.cache/android-wallet/api30-20260914` and the camera
evidence directory's `compact-*` files.

Both ABI builds, JVM tests, strict debug/release lint, final fixture isolation,
APK alignment and architecture pass. No production C changes in this slice.
Continue API36 runtime qualification and compact capture/review reachability.
The first one-core API36 boot hit a system-server watchdog under 99% guest CPU
pressure before any wallet APK was installed, then its emulator process faulted
during requested shutdown. Logs and the profile are preserved; a four-core
restart is in progress without changing watchdog or security policy. Physical
optics and hardware-authenticated custody remain separate unfinished gates.

2026-09-14 compact capture/review continuation: extending the same insets-aware
fixture proves two more failures in 25.918 seconds: capture Cancel is hidden,
and the public review address is unreachable. The chooser fixture stays green.
All three states now reuse one scrollable body. Capture retains a 160 dp base
preview, expanding in tall windows, while review scrolls its request and actions
together. The camera/decoder ownership, C packets, deadlines, overlay refusal,
network choice and saved-state policy do not change.

The exact same three-test APK passes in 27.813 seconds on API30. Real Camera2
review, Activity recreation and explicit rescan pass in 70.541 seconds with
preview/worker cleanup. The final minified API30 permission and public-camera
journey passes in 25.220 seconds. The existing API35 profile passes the three
compact layouts, network recreation/background state and three actual camera
frame/background/resume cycles together in 177.764 seconds. Both ABI builds,
JVM tests, strict lint, fixture isolation, native alignment and architecture
pass. Twenty unsigned release entries compare identically after local signing;
final APK, both fixture APKs and PNG hashes recheck. There is no C change.
Evidence is under the camera directory's `compact-all-*` files and
`.cache/android-wallet/api30-20260914`.

API36's preserved profile completes the four-core boot in 513 seconds. Its
first minified test fails in 71.296 seconds at the initial network selector
because Quickstep's ANR is the foreground Android window. A normal Wait action
then reveals a stacked System UI ANR; that startup condition is being cleared
before rerunning the final APK. No watchdog, permission assertion or overlay
filter was relaxed. Continue with final API36 minified and native/JNI runtime
acceptance, then the remaining security and usability work. Physical optics,
hardware-authenticated custody and real-source networking remain unqualified.

2026-09-14 API36 runtime acceptance: the final minified APK passes the complete
permission denial/retry/grant and exact Camera2 public review twice in 41.132
and 40.001 seconds. Eighteen debug native, layout, lifecycle and camera tests
pass together in 214.390 seconds, including real GCM record/store fixtures,
Keystore policy refusal, pending-open ownership, all compact states and actual
camera review across Activity recreation. These are public fixtures on a fresh
AOSP Android 16 x86_64 profile with 4096-byte pages; no wallet session was opened.

After the retained startup ANRs, an initial final-APK attempt fails at injected
permission denial: InputDispatcher reports a dropped touch on a permission
input sink without an input channel. Normal denial and resetting this owned
profile's permission flags permit the same unchanged test to pass; a second
complete run passes after restoring the same minified APK. No permission,
overlay or hardware-custody assertion changes. The no-wallet-directory guard
remains enforced. Logs are under `.cache/android-wallet/api36-20260914`, with
final input identities in the camera directory's compact-all signing proof.
Continue native JNI exception and secret-cleanup coverage. Physical optics,
hardware-authenticated custody and real-source networking remain unqualified.

2026-09-14 wallet-header JNI cleanup continuation: the existing key-entry host
harness and fuzzer now include header creation and recovered-address derivation.
They check exact public results, caller input preservation, every VM exception
ordinal, allocation refusal without an exception, pending/NULL inputs, malformed
headers, mismatched entropy and RNG failure after partial output. Touched entropy
and blinding spans must be cleared while live. Public header/address bytes have
separate obligations; the five existing key entries retain their prior checks.

All 61 native ASan/UBSan/LSan tests pass in 44.49 seconds. Clang/GCC analysis
passes, including both modes of the changed fixture, with unchanged 10/15
complexity caps. Bounded JNI fuzzing completes 32,362 executions in 121 seconds
without a finding. Four isolated mutants, each omitting one entropy or blinding
clear from one wallet-header entry, all fail the cleanup assertion. The first
safety invocation met the script's non-executable mode; explicit bash ran the
unchanged script successfully. Production C and custody policy do not change.
Evidence is under `.cache/android-wallet/jni-header-20260914`, and the explicit
hazard review is in `C_SAFETY_REVIEW.md`. Continue with the record object-array
JNI boundary and its bounded references/exception paths.

2026-09-14 wallet-record JNI continuation: a separate registered fixture now
checks pack/unpack's eight/sixteen VM call ordinals, both networks and all five
entropy sizes, exact component copies, malformed records, caller input
preservation and a peak of two live locals. It covers pending/NULL arguments,
partial reads/publication, NULL allocations without an exception and returned
references with a pending exception. JNI return-frame cleanup is distinguished
from prompt temporary-local deletion. No production C changes.

All 62 native ASan/UBSan/LSan tests pass in 44.69 seconds. Enabled-code analysis
and separate unit/fuzz fixture analysis pass in Clang and GCC, with all 832
fixture functions within the unchanged cap. Three isolated mutants remove the
class-local release, part-local release or object-publication exception check;
all are rejected. Bounded fuzzing completes 376,843 executions in 121 seconds
without a finding. The first attempt to build the new Makefile target required
explicit CMake regeneration; the configured target then builds and passes.

The new in-memory Android test passes all ten record profiles on API35 in
11.708 seconds and API36 in 4.674 seconds. It checks independent JNI arrays,
malformed-call recovery, real GCM and changed-tag refusal without a wallet
directory or Keystore alias. API35 output was initially delayed, then the
unchanged invocation completed successfully. Debug/test builds, strict
debug/release lint, fixture isolation, native APK alignment and architecture
pass. Exact device APKs are saved and their hashes recheck; the prior final
minified artifact identities also recheck, and that APK is restored on API36.
Evidence is under `.cache/android-wallet/jni-record-20260914`; the complete
hazard review is in `C_SAFETY_REVIEW.md`. Continue wallet lifetime and foreground
review safety. Physical optics, hardware-authenticated custody and real-source
networking remain unqualified.

2026-09-14 worker-submission cleanup continuation: two new host regressions
prove that thread-factory failure previously left transferred input uncleared,
including a task already inserted into the queue. `OwnedExecutor` now clears
input if task construction fails, and removes/discards failed handoffs before
preserving the original unexpected exception. Ordinary rejection still returns
false. Close polls its bounded queue directly without allocating a drain list.
The per-task claim still prevents execution or duplicate cleanup after discard.

Both regressions fail on the old code and all four executor tests pass after
the fix. The full JVM suites, debug/release/test builds and strict lint pass.
Real Android thread-factory fixtures cover both direct-start and queued-start
branches on API35 in 1.089 seconds and API36 in 0.406 seconds, including an
independent successful retry. They inject a synthetic exception without actual
memory exhaustion; production has no injection hook. The guarded API35 custody
refusal flow also passes in 34.957 seconds: its generated key is rejected, no
wallet is created and no recovery view appears. Its own fixture state is cleaned.

The new normal minified APK passes the full API36 permission denial/retry/grant
and public camera review in 38.683 seconds. All twenty unsigned release entries
are identical after local development signing. Saved device/minified inputs
recheck, and fixture isolation, both ABI page alignment and architecture pass.
There is no C or custody-policy change. Evidence is under
`.cache/android-wallet/worker-submission-20260914`. Continue secret-view input
transfer failure cleanup; physical custody and real-source networking remain
separate unfinished acceptance gates.

2026-09-14 secret-view failure cleanup continuation: public text-listener faults
reproduce three failures on the existing Android 16 APK in 9.130 seconds. Failed
append retains four input characters, failed delete retains two, and a rendering
failure does not attempt preview cleanup. Recovery input now clears its entire
owned buffer after either rendering failure and preserves any secondary cleanup
exception with the original. Input transfer clears the preview before allocating
the outgoing copy, with source clearing in finally even if allocation fails.
No secret String, IME, clipboard or new retained owner is introduced.

The same seven-test APK passes after the fix on API36 in 9.010 seconds and API35
in 18.072 seconds, including original secret-view/queue coverage, retry after
failed transfer and the three regressions. The full JVM suites, debug/release
builds and strict lint pass; fixture isolation and both ABI page alignment pass.
Exact debug/test APKs and the new unsigned release are retained under
`.cache/android-wallet/secret-view-20260914`, with hashes rechecked. This slice
has no C or custody-policy change. Continue testing stale framework hierarchy
restoration of recovery views; physical custody and networking remain separate.

2026-09-14 recovery hierarchy refusal continuation: the final ten-test fixture
reproduces three failures on API36 in 10.402 seconds. A saved ordinary TextView
record populates a fresh recovery display with 28 public marker characters;
restoring a populated display retains its owned words; keyboard restoration
leaves three current input characters. The keyboard observation is retained
input, not a framework exception. Both recovery views now explicitly omit
hierarchy saving and clear current material without reading any supplied state.

The same exact ten-test APK passes after the guards on API36 in 10.325 seconds
and API35 in 21.881 seconds. The full JVM suites, debug/release builds and strict
lint pass, along with fixture isolation and both ABI page alignment. Twenty
unsigned release entries compare identically after local development signing;
saved input hashes recheck. The final minified APK passes the full API36 real
permission and Camera2 review flow in 37.921 seconds. No C, custody-policy,
authentication or scanner behavior changes. Evidence is under
`.cache/android-wallet/secret-state-20260914`. The preserved API30 fixture is
restarting with four software-emulated cores for current runtime coverage.
Continue lifecycle cleanup when UI teardown fails; physical custody and
real-source networking remain unqualified.

2026-09-14 pause cleanup continuation: a guarded activity fixture reproduces an
open foreground worker after waiting-screen rendering throws, in 29.592 seconds
on API36. Pause now detaches and closes the session before rendering, with nested
finally blocks retaining timer/view cleanup and the framework pause call. Queued
public marker input is discarded immediately; active work keeps its existing
completion/finalizer path. No authentication or persistence authority changes.

The same test APK passes on API30 in 18.498 seconds, API35 in 43.153 seconds and
API36 in 27.355 seconds. It invokes the actual pause method inside a controlled
instrumentation callback, holds one bounded worker task, checks cleared queued
input/no queued execution and waits for cleanup. Its existing fresh-emulator
guard and invocation-owned cleanup remain intact; no key or wallet is created.
The preserved API30 profile also passes all twelve recent record/GCM, worker
and secret-view tests in 24.218 seconds after a four-core boot of 260.778 seconds.

Full JVM suites, debug/release builds, strict lint, fixture isolation and both
ABI page alignment pass. All twenty unsigned release entries compare identically
after local development signing, and exact saved input hashes recheck. The new
minified API30 camera fixture first fails in 52.037 seconds because a System UI
ANR covers the initial network selector; the window and hierarchy dumps identify
com.android.systemui. A normal Wait action permits the unchanged complete
permission/grant/Camera2/review fixture to pass in 28.745 seconds. No watchdog,
permission check or test assertion was changed. Evidence is under
`.cache/android-wallet/pause-cleanup-20260914` and the preceding secret-state
directory. Continue bounded worker ownership and remaining resource/lifetime
checks; hardware-authenticated custody and real-source networking remain open.

2026-09-14 process worker budget continuation: a host regression first proves
that a third executor could start while two closed sessions still had active
tasks. Admission is now limited to two process-wide owners, acquired lazily on
first submission and retained through active work/final cleanup until pool
termination. Per-owner limits remain one worker and four queued tasks. Admission
never waits or automatically retries; rejected input clears immediately and
the activity offers a busy message with explicit retry. Never-admitted owners
have only empty session state and clear it directly on close.

All five host executor tests pass, including capacity retained during finalizers,
exactly-once input cleanup and reuse after termination. The full JVM suites,
debug/release/test builds and strict lint pass. Three Android checks cover actual
pause/resume with repeated busy retry and later welcome recovery, prior failed
pause-rendering cleanup, and real ThreadPoolExecutor startup failures. They pass
on API30 in 32.456 seconds, API35 in 85.439 seconds and API36 in 63.612 seconds.
The guarded activity cases create no wallet/key and preserve fixture ownership.

Fixture isolation and both ABI page alignment pass; twenty unsigned release
entries compare identically after local development signing and saved hashes
recheck. The minified API30 full permission/Camera2/public-review journey passes
in 27.926 seconds. API36 attempts fail in 28.121 and 25.001 seconds at the grant
touch: InputDispatcher identifies the permission ActivityRecordInputSink with
NO_INPUT_CHANNEL and drops the injected ACTION_DOWN. Both failure logs, window
state and public hierarchy are preserved; no permission guard/assertion changes.
The normal denial action and owned-profile permission-flag reset precede the
unchanged retry. This artifact's API36 full camera journey is not qualified by
those attempts. Its separate worker/lifecycle tests pass as stated above.

Evidence is under `.cache/android-wallet/worker-budget-20260914`. Parent Git
fetch succeeds without recursive submodule fetching after Tor's referenced
object is unavailable upstream; the newly fetched parent commits do not change
this app. No merge, push, C, custody-policy or consensus change. Continue native
public QR/payment JNI fault coverage and bounded resource/lifetime review.

2026-09-14 public QR/payment JNI continuation: a new host fixture/fuzzer checks
exact copied projections for both networks and original P2PKH/P2SH vectors,
maximum 449-byte payment records, partial VM reads/publication, every VM fault
ordinal, NULL allocations without exceptions, returned references with pending
exceptions and pending-entry refusal. Fixed input snapshots, result canaries
and stop counts bound the fake VM. The C result is its projection reference;
independent symbol decoding remains a separate device/JVM claim.

All 63 native ASan/UBSan/LSan tests pass in 44.78 seconds. Clang/GCC analysis
passes for enabled code and separately for unit/fuzz fixture modes. All 456
production and 848 fixture functions remain within the unchanged 10/15 caps.
Four source mutants alter width, amount byte order, message presence or the
allocation exception guard; all fail the new fixture. The bounded fuzzer
completes 423,876 executions in 121 seconds without a finding. The new evidence
directory was corrected before the first safety invocation could start.

Four Android tests pass on API30 in 5.500 seconds, API35 in 32.611 seconds and
API36 in 20.359 seconds. Public payment tests independently expect the maximum
amount, a 200-byte UTF-8 label, a 200-byte message, copied ownership, malformed
and wrong-network refusal followed by a successful request. Canvas decoding now
includes P2SH on both networks at three aspect ratios while retaining both
P2PKH cases and the undersized-view refusal. No wallet, key, camera or endpoint
is accessed by these tests.

The full JVM suites, debug/release/test builds, strict lint, fixture isolation,
both ABI page alignment and architecture pass. The unsigned release is
byte-identical to the prior worker-budget artifact; production code is unchanged.
Exact saved APK/native fixture hashes recheck. Evidence and the explicit hazard
review are retained under `.cache/android-wallet/jni-public-20260914` and
`C_SAFETY_REVIEW.md`. Continue explicit secret ownership and lifecycle review;
hardware-authenticated custody, physical optics and real networking remain open.

2026-09-14 recovery-display handoff continuation: two real-Android regressions
fail on the prior APK in 1.170 seconds. A failure clearing the old display leaves
the incoming owned array untouched, and a second cleanup error replaces the
original rendering error. The view now consumes ownership at method entry,
clears incoming words on every failed replacement and retries empty rendering.
Secondary cleanup failure is retained with the original after array clearing.
The existing delivery callback guard already clears failed handoffs and remains
unchanged; this closes the view's own ownership gap.

The exact same test APK passes all twelve secret-view tests on API30 in 5.901
seconds, API35 in 23.216 seconds and API36 in 11.112 seconds. This includes saved
state refusal, keyboard/transfer cleanup, queue closure, both new display
regressions and successful fresh display after refusal. Public markers only;
no wallet/key, IME, clipboard, saved phrase or new secret owner is introduced.

Full JVM suites, debug/release builds, strict lint, fixture isolation and both
ABI page alignment pass. Twenty unsigned release entries compare identically
after local development signing; saved input hashes recheck. The final minified
API30 full permission/camera/public-review journey passes in 26.887 seconds, and
the same minified artifact is restored on API36. There is no C or custody-policy
change. Evidence is under `.cache/android-wallet/secret-display-20260914`.
Continue actual Android private-file refusal and authenticated pending-record
promotion in isolated public fixtures, preserving the hardware-custody gate.

2026-09-14 Android private-storage continuation: eight new device cases exercise
the existing C filesystem adapter in exclusively created temporary directories.
Public provider-GCM authentication and recovered-address validation precede
successful exact-byte pending promotion. Competing authenticated records refuse;
idempotent promotion rechecks committed bytes. Bad tags fail authentication,
corrupt committed files retain precedence, and nine truncated/oversized pending
lengths remain preserved recovery conditions rather than new-wallet permission.

Directory/file symlinks, unsafe permission bits and nonempty locks all refuse
without target writes. Correcting only fixture-owned metadata permits a fresh
valid call. Descriptor scans cover both the temporary root and its children
after calls. Four simultaneous Android creators yield exactly one complete
winner and only BUSY/ALREADY_EXISTS competitors, with exact retained winner
bytes and no pending file. Worker completion precedes fixture cleanup.

All eight tests pass on API30 in 4.162 seconds, API35 in 15.137 seconds and API36
in 8.124 seconds. Test build, strict Android lint, fixture isolation, architecture
and whitespace checks pass. Exact test/application APKs are saved and hashes
recheck; the application matches the already tested recovery-display APK byte
for byte. Production C/Kotlin and custody rules are unchanged. Evidence is under
`.cache/android-wallet/android-storage-20260914`. This is native Android
file/JNI evidence using public ciphertext, not hardware custody, malicious
rollback resistance or physical power-loss qualification.

2026-09-14 bounded public BLAKE2b-256 qualification: the preserved reference
candidate now has a separate two-line full-parameter initialization repair.
The untouched source still reproduces the original Clang finding; the exact
repair passes both Clang/GCC analyzers without suppression or changed gates.
Upstream keyed sanitizer self-tests and separate unkeyed/personalized libsodium
comparisons pass. Pre-integration provider fuzzing completes 4,085,206 executions
in 121 seconds. Original source, finding and evidence archive remain preserved.

The integrated helper accepts only public input up to 4096 bytes, exactly 16
personalization bytes, unkeyed sequential32-byte output and zero salt. It checks
all provider results, leaves output unchanged on failure and clears all owned
scratch. The normal suite now passes 65 tests under ASan/UBSan/LSan in 45.68
seconds. Clang/GCC fixture analysis passes in all four unit/fuzz/oracle modes;
production/test complexity remains within 10/15 (458/863 functions). Independent
56-vector generation reproduces exact committed bytes. Fault fixtures reject
six mutants, covering all three omitted wipes, missing personalization, ignored
init failure and publication after failed finalization. Wrapper fuzzing with
live libsodium comparison completes 6,676,084 executions in 121 seconds without
a finding. The explicit host oracle target also passes.

NDK ARM64/x86-64 build, JVM tests, Android lint, APK alignment/fixture isolation
and architecture pass. Standalone x86-64 tests linked from the actual Android
release core/provider archives pass 56 vectors at all 65 output capacities plus
span refusals on API 30,35,36; ARM64 is compiled only. Provider-only known-answer
and chunk fixtures also pass on all three. Shell fixtures contain only public
bytes and access no wallet/Keystore. Debug/test/unsigned release APK bytes are
unchanged from the preceding checkpoint because JNI does not reach the helper.
No redundant installation or camera rerun is represented as new evidence.

Full scope, hashes, reproduction and required hazard review are in
[BLAKE2_REVIEW.md](BLAKE2_REVIEW.md) and [C_SAFETY_REVIEW.md](C_SAFETY_REVIEW.md).
Evidence is retained under `.cache/android-wallet/blake2-init-20260914/` and
`.cache/android-wallet/blake2-wrapper-20260914/`. Original Zclassic branch-specific
signature hashes, authenticated review/key ownership and hardware custody remain
open. TLS quarantine and all consensus/custody boundaries remain unchanged.

2026-09-14 original signature-hash reference fixtures: a separate host C17
oracle matches all 130 untouched v4 expected hashes from pinned original
Zclassic commit 14a83d510ffd109d3fa09bf74ebf8c28854a263f. It uses an independent
bounded reader and host libsodium, including no wallet parser/serializer/hash
provider and compiling no original C++. It validates original signature-hash
bytes only; opaque proof/script data and historical branch values do not prove
transaction or current-chain validity.

Only after all original rows match does it emit 144 explicitly projected
SIGHASH_ALL cases using the existing rows 203/208/296, every selected input,
two public script profiles, three amounts and four explicit branch values.
The original dataset's amount is always zero; further independent nonzero
comparisons remain before wallet-code qualification. No actual wallet
signature-hash constructor, key or signing path is introduced in this slice.

Strict Clang/GCC analysis, ASan/UBSan/LSan and exact generated-header
reproduction pass. Ten malformed inputs and eight oracle mutations fail with
no expected output emitted. Existing-report refusal preserves all bytes and
its manifest. The full native safety suite passes 65 tests in 45.26 seconds,
with unchanged complexity caps (458 production/884 test functions). Host oracle
source, pinned originals, generated header, binary hashes, refusals and mutation
logs are retained in `.cache/android-wallet/sighash-oracle-20260914/`.
Android builds, JVM tests and lint also pass; debug/test/unsigned release APKs
remain byte-identical to the prior qualified artifacts. Architecture and
whitespace checks pass. No new Android instrumentation or custody result is
claimed for this host-only fixture milestone.

2026-09-14 independent nonzero-amount qualification: the oracle now requires
both published ZIP 243 transparent-input records in addition to all 130 pinned
original v4 vectors before emitting derived expectations. Exact selected TSV,
upstream MIT notice and source/extraction identities are committed; no Python
source or toolchain is fetched or invoked. The two published amounts exceed
uint32, and their expected digest bytes compare directly. Eight NOT_AN_INPUT
cases are explicitly outside this transparent-input fixture scope.

Three mutants reverse the amount bytes, truncate to uint32 or replace the
amount with zero. Each passes all 130 original zero-amount cases and is then
refused by the added independent comparisons with no generated output. Ten
malformed ZIP cases and all ten original refusal cases also withhold output.
The existing 144 projected digest values remain byte-identical; only their
provenance comment changes. Strict Clang/GCC analysis, ASan/UBSan/LSan, exact
regeneration and existing-report preservation pass. The full native suite
passes 65 tests in 45.09 seconds, with 887 test functions within the unchanged
complexity cap. Architecture passes. No Android production source changes or
new device acceptance claims occur in this host-only slice.

Evidence is preserved in `.cache/android-wallet/zip243-oracle-20260914/`.
The next gate is the bounded wallet signature-hash constructor against these
independent expected values, then exact authenticated review/key ownership.
Hash compatibility alone still does not grant signing or spending authority.

2026-09-14 bounded transparent SIGHASH_ALL: the internal C constructor now
matches all 144 independent expected values at every output capacity0..64.
Maximum 8-input/16-output profiles exercise 544-byte output components and
397-byte final preimages; scriptSig changes intentionally leave the v4 digest
unchanged. Amount, scriptCode, input index and branch are explicit caller data.
This computes raw hash bytes only, with no key, JNI, signature, branch selection,
wallet mutation or send action. It checks the complete existing transaction
profile before copying, checks all four provider results and clears its work
while preserving output on failure.

All 67 native ASan/UBSan/LSan groups pass in 45.23 seconds. Clang/GCC analysis and
unchanged 10/15 production/test complexity caps pass (467/912 functions).
The host oracle is shared without CLI globals in its library mode and still
reproduces all 130 original plus 2 ZIP comparisons and the exact 144-case header.
Four dirty provider failures prove stopped calls, unchanged output and cleanup
observed only during live object lifetime. Eleven mutants of hash context,
serialization, failure propagation, publication and full/partial cleanup fail
as intended. Bounded malformed-object/preimage fuzzing with the independent
reader/libsodium oracle completes 1,116,954 executions in 121 seconds without
a finding; explicit maximum-profile seeds participate.

NDK ARM64/x86-64 build, JVM tests, Android lint, APK/fixture isolation and
architecture gates pass. Standalone tests linked from actual Android release
archives pass all vectors, max profiles, capacities and refusals on API 30,35,36.
ARM64 is compiled only. ELF load alignment is 16 KiB with RELRO, immediate binding
and non-executable stack. Debug/test/unsigned release APK bytes are unchanged
because the new internal constructor has no JNI caller; these are standalone
C observations, not a new APK sending feature. Evidence is retained under
`.cache/android-wallet/sighash-core-20260914/`. Exact authenticated review/key
ownership and current branch/height binding remain the next acceptance gates.

2026-09-14 live unsigned-review digest binding: internal P2PKH SIGHASH_ALL now
takes the amount and exact standard script only from the owned assessment and
transaction bytes only from the owned canonical draft. Callers cannot replace
funding bytes, amount or script during hashing. P2SH selected inputs refuse.
The operation shares the existing exact-ID, fixed-deadline, rollback and clear
rules. It still has no key, JNI, signature, current-branch/height selection or
authenticated transaction approval. A digest alone cannot become authority.

Both networks, four explicit branch values, all 65 capacities, amount 0/1/MAX,
maximum 8-input/16-output profiles, caller-source destruction, mutable public
copies and every lifetime/refusal case pass. Independent reader/libsodium
comparisons agree. Three dirty provider-stage failures on two distinct inputs
preserve caller output and owner, stop subsequent work and clear live private
spans. All thirteen source/lifetime/cleanup mutants fail. The extended review
state-machine fuzzer completes 296,803 executions in 121 seconds without a
finding. Final Clang/GCC and all 69 native ASan/UBSan/LSan tests pass in 45.92
seconds, with unchanged 10/15 complexity limits. The initial GCC test-observer
count finding is preserved and repaired with an explicit bound before access.

Android/JVM/lint, APK/fixture isolation and architecture pass. Shared lifetime
extraction changes APK bytes, so fresh debug installations exercise all nine
native-review/lifecycle cases on API 30/35/36. Two initial combined clients hit
their 90-second host budgets; Android logs later record zero failures, and
separate complete reports pass all five/four cases within bounded 180-second
group runs. Original fixture assertions/timeouts remain unchanged. Actual
release-archive standalone C tests pass on all three x86-64 API levels; ARM64
is compiled only, with 16KiB ELF alignment and existing hardening on both ABIs.

The new locally signed minified APK passes API 30's complete permission-denial,
grant, public QR, review and camera-worker cleanup in 25.735 seconds. API 30/36
retain minified and API35 debug. Evidence and exact hashes are retained under
`.cache/android-wallet/review-sighash-20260914/`; full boundaries and manual
hazard review are in TRANSACTIONS.md and C_SAFETY_REVIEW.md. Next is authenticated
key/change ownership and durable state binding; branch/height authentication
and signing remain separate gates. TLS and hardware custody remain unqualified.

2026-09-14 consumed change-address reconstruction: C can now reconstruct an
explicit internal-chain address only below a complete authenticated observed
journal head, matched to the exact recovered wallet and head position. Unused,
missing, partial, corrupt, misplaced and wrong-wallet state refuses. Exhausted
state still permits previously consumed addresses. No wallet/journal bytes
change, no index is reserved again, and no key, signature or approval is returned.
The platform's exact-record GCM/per-use hardware prerequisite remains mandatory.

Both networks/all entropy widths, all 65 output capacities, 160 partial lengths,
all 80 head-byte corruptions, argument extremes and the final consumed index
pass. Dirty observation/MAC results, partial RNG/address output, caller-byte
mutation, all reached metadata/close failures, parent-sync/read refusals and
descriptor counts are tested. Blinding cleanup is observed only during live
lifetime. All twelve mutants fail; fuzzing completes 28,364 executions in 121
seconds without a finding and verifies journal preservation on every outcome.
The final Android-capable fixture and original reservation mode replay all 57
corpus files. Initial and rebuilt fuzz artifacts remain distinguished.

Final Clang/GCC, all 71 native ASan/UBSan/LSan groups (49.93 seconds), unchanged
10/15 complexity caps, Android/JVM/lint, artifact and architecture checks pass.
Standalone tests linked from actual release archives pass on API 30/35/36 using
shell-owned synthetic fixtures; ARM64 is compiled only. Three real-JNI public
record/storage/GCM cases also pass on every API after fresh debug installation,
including paired creation and wallet-only restoration. The minified artifact's
19 original archive entries remain unchanged after local signing; API 30/36
retain minified and API 35 debug. No new camera or hardware-custody result is
claimed. Evidence is retained in `.cache/android-wallet/change-ownership-20260914/`.

The next composition step matches authenticated receive/change ownership to
the exact live review inputs. Current branch/height, authorization and signing
remain separate gates. A fresh origin fetch advances main to 16fc6c6a1 without
Android-wallet changes; this work remains on the existing agent branch and no
push or merge occurs.

2026-09-14: the internal live-review wallet comparison now matches an exact
reviewed P2PKH input to the committed recovered wallet's v1 receive0 or a
previously consumed change key. It checks network and exact record identity,
refuses pending/unused/wrong-key claims, and clears its owned entropy/path/
record/comparison work on every entered exit. Platform GCM and per-use hardware
policy remain prerequisites; this internal check adds no JNI, key export,
signing or reusable authorization token. Its supplied-time check cannot stand
in for a future signer's completion-time lifetime and chain-context checks.

Both networks/all five entropy widths, maximum eight inputs, destroyed borrowed
sources, altered wallet/entropy, corrupted/truncated state, bounds/P2SH and
lifetime cases pass. Dirty-provider faults prove error propagation, private
source copying despite mutation, and live entropy/blinding cleanup. All15
mutants fail intended assertions. The independent lifetime/file-preservation
fuzzer completes21,928 executions in 121 seconds without a finding; its110-case
regression is registered. Clang/GCC analysis and all74 ASan/UBSan/LSan groups
pass in55.23 seconds; complexity caps stay10/15 (479/1001 functions). Optimized
host frame evidence is1608 bytes. The initial exclusive fixture-writer error
was corrected by unlinking only the owned synthetic journal before recreation,
without weakening that writer's guard. Initial/final evidence remains intact.

Android/JVM/lint, APK alignment/fixture isolation and architecture gates pass.
The debug/unsigned-release/test APK hashes are byte-identical to the preceding
consumed-change milestone because the added operation is unused by JNI.
Actual release-archive standalone tests pass on x86-64 API30/35/36; no app
wallet, Keystore or preferences are touched. Executable SHA256 is
`3c410fb79d1f4b1ee7ebb9f27d37c24c5e8b9e09d0dde3293332a3b352971375`.
ARM64 is compiled only; both standalone ELFs satisfy16KiB alignment, RELRO/NOW
and non-executable stack checks. Exact source/archive/artifact/device evidence
is in `.cache/android-wallet/review-ownership-20260914/`; review and scope are
in `C_SAFETY_REVIEW.md` and `TRANSACTIONS.md`.

Fetched origin/main remains `16fc6c6a13135950e450c5b3156a125ba8682728`, with no
upstream Android app changes; the existing working branch is preserved without
merging or pushing. Next: current branch/height and platform-authenticated
context bound to the exact live review, then synthetic signing/cancellation.
The owner-parked TLS investigation remains excluded and unqualified; software
emulators do not supply positive hardware-custody acceptance.

2026-09-14: internal candidate-context hashing now selects the pinned Zclassic
v4 branch from an explicit candidate height, matches network, checks the owned
review's expiry/lock/sequences, then computes its P2PKH digest. Expiry equality
and zero expiry retain their original meaning; lock equality requires all
input sequences to be final. All eight inputs participate in finality. No
transaction field, next height, expiry horizon or relay policy is chosen
implicitly. Candidate metadata copies before provider calls and private work
clears after every entered path. This is public preflight data, not current
chain authentication, user consent, ownership or authority to sign/broadcast.

A bounded offline projector verifies four exact original Git objects at
`14a83d510ffd109d3fa09bf74ebf8c28854a263f` and extracts all epoch IDs and both
network schedules. A second projection matches the checked-in header/checksum;
existing output directories refuse without changing their contents. Independent
traversal compares1,600,002 consecutive network/height pairs plus integer edges.
The original C++/node is not executed. Boundary/capacity/expiry/finality tests,
all-eight-input and destroyed-source cases, dirty-provider/source-mutation
faults, output preservation and live cleanup pass. Optional independent
reader/libsodium digest comparisons also pass. All19 mutants fail intended
assertions. The extended review fuzzer completes327,566 executions in 121 seconds
without a finding. All77 native ASan/UBSan/LSan groups pass in54.98 seconds,
Clang/GCC analysis passes, and complexity caps stay10/15 (484/1027 functions).
Optimized host frames are0 bytes for lookup and120 for the context wrapper.

Android/JVM/lint, APK alignment/fixture isolation and architecture checks pass.
Debug/unsigned-release/test APK bytes remain identical because no new JNI
caller exists. Both standalone tests link the actual new release archives and
pass on x86-64 API30/35/36. Runtime executable SHA256 values are
`d5520a22c4755356ef7af995a172485c766f430e5ec35b10af445c09ee0e68e0`
(branch traversal) and
`b5a831c0c6d6393f3bd8848516642afbb56eb39b747043c73735f833cda11147`
(review context). ARM64 is compiled only. All four standalone ELFs have16KiB
alignment, RELRO/NOW and non-executable stacks. Public fixture tests access no
app wallet, Keystore or endpoint. Complete evidence and source/archive/artifact
hashes are in `.cache/android-wallet/review-context-20260914/`.

Fetched origin/main advanced to `ee670af5225bd1f62822ec0459a42a0706a862f1`;
there are no upstream Android app changes and the existing branch remains
unmerged/unpushed. Next work remains platform-authenticated context/consent and
synthetic signing/cancellation tied to the exact review. The owner-parked TLS
investigation and positive hardware-custody acceptance remain unqualified.

2026-09-14: internal synthetic signing now copies exactly32-byte secret/digest
inputs before OS/provider work, obtains fresh OS blinding, and produces
deterministic RFC6979 low-S DER with a compressed public key. Nonce work is
bounded to eight candidates. Parsed output and exact-digest verification precede
whole-output publication; every failure preserves output and all entered paths
clear private work and the checked transient EC allocation. No JNI, wallet
unlock, script/wire publication or authorization token is added.

The64 profile/repetition suite, scalar/message-order edges, changed-digest and
all argument/guard checks pass. OpenSSL independently derives the key, validates
strict DER/low-S and verifies the raw digest. Dirty stage failures, caller-source
mutation, RNG/OOM/context errors, nonce bounds and live zeroization pass.
All24 deliberate defects are detected:23 intended assertions and one UBSan
nonnull call interception. The mutation runner's initial assertion-only
expectation was corrected to record that earlier detection. GCC's initial test
provider annotation conflict was fixed using upstream's implementation mode,
preserving runtime NULL checks and production caller annotations. No security
assertion, warning or sanitizer is suppressed; all initial logs are retained.

The OpenSSL-enabled fuzzer completes36,242 runs in 121 seconds without a finding.
All79 native ASan/UBSan/LSan groups pass in55.70 seconds; Clang/GCC analysis and
unchanged10/15 complexity caps pass (490/1066 functions). Optimized host entry
and nonce-callback frames measure552/8 bytes. Android/JVM/lint, APK
alignment/fixture isolation and architecture checks pass. Actual new release
archive tests pass on x86-64 API30/35/36; ARM64 is compiled only. Runtime
executable SHA256 is
`0f2c40db0e05547cd3e00f0897b3d59877aa0fcdcd0490f2a71e165d80f32ce2`.
Both standalone ELFs have16KiB alignment, RELRO/NOW and non-executable stacks.
APK bytes remain unchanged because the primitive has no JNI caller; no fresh
camera or hardware-custody claim is made. Public synthetic device fixtures touch
no app wallet, Keystore, endpoint or node. Full evidence and source/archive/
artifact identities are in `.cache/android-wallet/signature-core-20260914/`.

Fetched origin/main is `7f5fe466b6da49b62343859068dc2872c7f43d79`, with no
upstream Android-wallet changes. The existing branch remains unmerged/unpushed.
Next: canonical signed wire and live-review completion/cancellation composition,
with platform-authenticated context/consent still required before any wallet
signing adapter. TLS stays owner-parked; real funds remain outside fixtures.

2026-09-14: public signature verification now produces canonical P2PKH input
scripts only after matching the compressed key's HASH160, strict DER/low-S and
the exact supplied digest. All inputs copy before provider work; only complete
success publishes the two minimal direct pushes and length. The fixed
SIGHASH_ALL byte and33-byte key give DER+36 bytes, bounded44..107. No heap,
private key, RNG, JNI or authorization is introduced. Original Zclassic
signature-byte/stack-order/direct-push source rules were inspected at the pinned
commit; no original node or script interpreter is executed.

The64-profile suite passes129 capacities plus SIZE_MAX, known generator hash,
all used-byte mutations, scalar/DER/length/NULL errors and guard checks. OpenSSL
independently verifies the hash/DER/low-S/digest relation. Dirty-provider and
source-mutation tests preserve both outputs and clear captured live work.
All24 mutants are detected (23 assertions, one ASan SIZE_MAX comparison).
All81 native ASan/UBSan/LSan groups pass in58.04 seconds; Clang/GCC analysis and
unchanged10/15 complexity caps pass (495/1095 functions). The optimized host
entry frame is600 bytes. GCC's test-provider array declaration mismatch and
the fuzzer's complexity/local-bound findings were fixed while retaining every
warning, assertion and cap. Initial/final evidence remains separate.

Android/JVM/lint, APK alignment/fixture isolation and architecture gates pass.
APKs remain identical because no JNI caller exists. Actual release-archive
tests pass on x86-64 API30/35/36; ARM64 is compiled only. Runtime executable
SHA256 is `e5f518f3120cbe9e96d24676d708c454ab6d35df656a35634d3581cc8cc34477`.
Both ELFs have16KiB alignment, RELRO/NOW and non-executable stacks. Public
synthetic tests access no app wallet, Keystore, endpoint or node. Evidence and
source/archive/artifact identities are in
`.cache/android-wallet/signature-script-20260914/`.

Fetched origin/main remains `7f5fe466b6da49b62343859068dc2872c7f43d79`, with no
Android app changes; the existing branch is unmerged/unpushed. Next is complete
signed wire bound to the exact live review, followed by platform-authenticated
context/consent and completion/cancellation. TLS remains owner-parked, and no
positive hardware-custody or real-funds acceptance is claimed.

The final OpenSSL differential script fuzzer completes1,792,794 cases in 121
seconds without a finding, with max_len160, timeout5 and RSS512MiB
(peak274MiB). Initial698,807-run evidence remains separately attributable to
the earlier harness; the final binary and source identity are checked explicitly.

2026-09-14: complete public signed-wire assembly now binds every P2PKH
signature to its exact owned review input and explicit candidate. It preserves
all original transaction fields except input scripts, stages bounded wire
privately, rechecks liveness around encoding and publishes bytes/length only
on complete success. All copied work clears. No secret, RNG, heap, JNI, consent,
current-chain authentication or broadcast authority is added. Completion-time
freshness remains the future authorized adapter's responsibility.

Real-provider and independent host tests cover both networks, 1..8 inputs,
1/16 outputs, capacities, source destruction, altered signatures and review
lifetime transitions. The 20-mode dirty-provider suite also covers cancellation,
replacement, expiry and rollback during encoding. All 26 mutants are detected
(25 assertions and one ASan negative-size copy). All 83 ASan/UBSan/LSan groups
pass in 61.24 seconds; Clang/GCC analyses and unchanged 10/15 complexity caps
pass (500/1145 functions). The independent wire/state fuzzer completes 74,095
cases in 121 seconds without a finding. Individual optimized entry/publication
frames measure 3304/2040 bytes, within the existing 4096-byte per-frame gate.

Android/JVM/lint, alignment/fixture isolation and architecture checks pass.
Actual release-archive tests pass on x86-64 API 30/35/36; ARM64 is compiled only.
Runtime executable SHA256 is
`1e2976628ef3c505fc3cc19d7fb72392b60dc7bb5909ec00153a7954f91c9a4c`.
Debug and test APKs remain identical. Both release libraries differ only in
GNU build ID notes; comparison copies with just those notes removed match
byte-for-byte. The unsigned release SHA256 is
`1126be153726695456f32138d75da8b7b7328fab3b4b958ec7718cb133cbb402`,
and its locally signed minified APK is
`5d7beeb5c95b435e4f031ecd67a19071a735287934a5afb1aa4b8f7fa9bd30c0`.
An extra attempt to run debug instrumentation against minified release failed
because its direct Kotlin-class dependencies were removed by shrinking. This
was a test-target mismatch; no production keep rule or security check was
weakened. Corrected evidence distinguishes debug JNI tests and minified startup
from the standalone release C acceptance. No fresh camera or positive hardware
custody acceptance is claimed. Exact logs, sources and artifacts remain in
`.cache/android-wallet/review-signed-wire-20260914/`.

Fetched origin/main is `b240f40c77bb56f172b0223d0eb2a074cf8409a3`, with no new
upstream Android app commits. The existing branch remains unmerged/unpushed.
Next is the user-requested emulator ADB reaping repair, followed by authenticated
context/consent and completion/cancellation composition. The observed 26 old
ADB zombies belong to the three running emulators; signed-wire device runs add
none. Existing emulator data remains intact. TLS stays owner-parked and real
funds stay outside all development fixtures.

2026-09-14: the emulator ADB zombie cause is reproduced and repaired for the
exact measured Linux x86-64 SDK37.1.11. Its private timeout path kills its child
then polls once with WNOHANG, which can return before the kernel releases the
child. A hash-qualified local launcher enables an isolated C17 ELF adapter
that completes only that precise owned post-SIGKILL wait. Other child owners,
options, status and errno semantics stay intact. SDK files, Android binaries,
wallet data and consensus remain untouched.

Final traces prove27 completed forced timeout reaps plus12 in a second run
that also exercises shutdown with an in-flight child and exits0. Two real-ADB
cycles exit0. An intentionally infinite stand-in initially outlived an exiting
SDK and kept strace alive; its final fixture has parent-death handling and a
30-second alarm. Initial escalation results remain recorded separately.
The26 old zombies belong to unrelated live emulators; their processes and data
are preserved, and the new cycles add none.

All85 native sanitizer groups pass in61.37 seconds. Clang/GCC analysis,
unchanged10/15 complexity caps, all16 mutation checks, Android/JVM, lint,
scanner fixture and architecture checks pass. A bounded differential fuzzer
completes71,385,647 cases in 121 seconds without a finding. The current minified
APK passes the full public camera permission/retry/review/resource-release
fixture on isolated API30 in9.864 seconds; the release-archive signed-wire C
test also passes there. No hardware custody or physical-camera claim is made.
Exact identities, measured boundaries and source review are in
[EMULATOR_REAPING.md](EMULATOR_REAPING.md), [C_SAFETY_REVIEW.md](C_SAFETY_REVIEW.md)
and `.cache/android-wallet/adb-reaping-20260914/`. Continue with wallet
authorization/context and delivery lifetime safety; TLS remains owner-parked.

2026-09-14: wallet session close no longer submits a new worker task solely for
cleanup. A public regression reproduces the old failure: a thread factory that
refuses a replacement can leave cleanup queued forever; a throwing factory can
skip it. The private pool now owns cleanup in its termination hook, after all
active input cleanup and before releasing process admission. An idle pool with
no remaining worker clears directly. The callback reference retires before
invocation; no new thread, queue slot, retry, secret copy or authority is added.
`WalletPlatformSession` documents that exclusive final ownership explicitly.

All seven executor unit tests pass, including both idle-pool start modes,
NULL/OutOfMemoryError/SecurityException thread-factory faults, idempotent close,
failed finalization, both process admissions, queued input clearing and admission
held through active/final cleanup. An isolated Kotlin/JUnit build with warnings
as errors also passes; five mutations (missing/double cleanup, early/missing
admission release and the old close implementation) fail intended assertions.
No production injection hook or weakened test is introduced.

The real Android executor cases pass on API30/35/36 in0.025/2.327/1.144 seconds.
The isolated API30 Activity cases also pass in3.358 seconds: repeated busy
pause/resume with explicit retry, and a thrown pause-rendering callback with
worker/input cleanup preserved. They create no wallet/key. Full Android/JVM,
debug/release/test builds, strict lint, fixture isolation and16KiB native APK
alignment pass. Both ABI native libraries remain byte-identical to the previous
signed-wire milestone; this slice changes only managed platform lifetimes.
Evidence is in `.cache/android-wallet/executor-close-20260914/`.

The current locally signed minified APK is
`8ed2a06c7cf316291c11c22bca84e015d33d153eb6ce4555fce77efd0398c8f1`;
all20 unsigned release entries compare byte-for-byte after signing. Its complete
camera denial/retry/grant/public-review/resource-release fixture passes in9.455
seconds on a fresh isolated API30 profile. An earlier reused profile failed
because its actual dialog offered `permission_deny_and_dont_ask_again_button`,
instead of the fixture's required first denial; the hierarchy is retained.
API30 lacks the requested `clear-permission-flags` shell command. A new separate
AVD provides clean initial permission state without erasing the old profile or
changing an assertion. Both new launches use the qualified ADB reaping adapter,
exit0 through console shutdown, and leave the original26 zombies unchanged.
Current origin/main is `9bc54830ad4bcd7a9a0959aaf4ffc53ec0726db2`, with no
upstream Android app commits. No push, merge or hardware-custody claim is made.

2026-09-14: wallet record reads and promotion now require exactly one filesystem
link, matching the existing lock/change-journal policy. A real-alias regression
fails on the old code. The descriptor check now refuses aliases and detached
metadata before reading, preserving all caller outputs and stored bytes/names.
No repair, deletion, overwrite, allocation, new retry or consensus change is
introduced. The private-directory/cooperating-lock and GCM prerequisites remain.

The two new mandatory native groups cover real linked committed/pending records
and controlled0/1/2/native negative-one link metadata, with exact output/inode
preservation and successful single-link retries. All87 ASan/UBSan/LSan groups
pass in61.38 seconds. Clang/GCC analysis and unchanged10/15 complexity limits
pass (500/1160 functions). Four link-rule mutants fail intended assertions.
The new bounded filesystem fuzzer completes305,619 cases in 121 seconds without
a finding, with142-byte inputs,5-second cases and512MiB RSS cap (observed77MiB).
Another10,000 cases pass with a64-descriptor limit. It models storage policy and
reuses the record parser for content status; it is not an independent codec oracle.

The exact release-archive metadata executable passes on x86-64 API30/35/36;
ARM64 is compiled only. Runtime SHA256 is
`6528afa300ba461c2029ba8e93af5728c2117bf38dcf6d830c740192fe211667`.
Attempts to construct a FIFO and real hard links on Android fail at the kernel
fixture boundary. The complete Linux assertions remain mandatory; Android
metadata evidence is stated separately and no platform policy is disabled.
Two invocation-owned JNI storage/provider-GCM tests pass on each API in
1.889/18.396/5.302 seconds. No operator wallet or Keystore alias is touched.

Android/JVM, debug/release/test builds, strict lint, fixture isolation and16KiB
native APK alignment pass. The current locally signed minified APK is
`5a9e8f8db1d07307a9ed17cb5f65aab7e9cf620f7343fdecbab45f296bdb6ddf`.
Evidence, initial failures and final source/archive/artifact identities are in
`.cache/android-wallet/storage-links-20260914/`; the full hazard review is in
[C_SAFETY_REVIEW.md](C_SAFETY_REVIEW.md). Continue custody/lifetime review and
authenticated transaction composition; hardware custody and real-source
networking remain unqualified, with TLS owner-parked.

All20 unsigned release entries remain identical after development signing. The
exact minified APK starts through its normal exported launcher on API30/36.
An initial external launch of the private scanner Activity is refused by its
existing nonexported boundary; no manifest change is made. A separate owned
shell fixture confirms Permission denied for Android FIFO and hard-link
creation and removes only its own three fixed names/directory. Final ADB zombie
count remains26 under the original unrelated emulators. Current origin/main
remains `9bc54830ad4bcd7a9a0959aaf4ffc53ec0726db2`; no upstream Android app
commits, push or merge are involved.

2026-09-14: camera worker construction/start failure now returns camera admission
through a pre-handler `finally` guard. Synthetic OutOfMemoryError cases reproduce
both leaks on the old implementation. The failed owner closes permanently, idle
worker shutdown is requested, and fatal errors still propagate. Ordinary
SecurityException failures still report once. An immutable internal worker
factory permits controlled construction/start faults without exhausting memory
or opening an OS camera; existing production call sites keep their constructor.
The handler-published and pending OS-open lifetime rules are unchanged.

Both regression tests pass on API 35 and 36 in 1.260/0.468 seconds. On fresh API 30,
the same cases plus real pending-open cancellation, three background/restart
cycles, and recreation pass together (five tests, 52.053 seconds). The separate
denied-permission worker cleanup test passes in 0.012 seconds. No test opens a
wallet, obtains a key, changes a custody policy, or weakens a camera timeout.

Android/JVM tests, debug/release/test builds, strict lint, fixture isolation,
architecture placement and 16 KiB native APK alignment pass. Both native ABI
libraries are byte-identical to the storage-link milestone, whose 87 sanitizer
groups and bounded storage fuzzer remain the applicable native evidence.
The locally signed minified APK is
`25b29ca27bf7e522d3255cebd08263906166377776c02200894188f44df7956a`;
all 20 unsigned entries compare exactly after signing. Its complete camera
denial/retry/grant/public-review/resource-release acceptance passes on fresh
API 30 in 9.382 seconds. Before/after artifacts and logs are retained in
`.cache/android-wallet/camera-start-20260914/`. The isolated emulator uses the
qualified ADB reaping wrapper; the original 26 zombies remain under their
unchanged live parents, with none added by this validation.

2026-09-14: camera preview failures now retire their image and clear copied
pixels before Android bitmap cleanup can throw. Upload scratch clears on both
success and failure; rejected packets remove the older preview. Cleanup reuses
the view's existing empty buffer and still invokes framework detach cleanup.
The app does not recycle a bitmap that rendering may still reference and does
not claim erasure of every provider/rendering copy.

Four regressions fail on the old code: rejected bitmap erase retains scratch,
immutable bitmap upload retains copied pixels, malformed input retains an older
image, and clearing allocates a fresh empty buffer. All four pass on API 35/36
in 9.163/4.579 seconds. The fixtures use public pixels and real Android bitmap
refusals in unattached views; they neither open a camera nor exhaust memory.
Minimum/maximum frame dimensions, unchanged borrowed input, idempotent cleanup
and successful retry are covered. On fresh API 30, those four cases plus exact
camera QR review, background/restart cycles and recreation pass together:
seven tests in 97.883 seconds.

Android/JVM suites, debug/release/test builds, strict lint, fixture isolation,
architecture and 16 KiB native alignment pass. Both ABI libraries remain
byte-identical to the storage-link milestone; no C or consensus logic changed.
The locally signed minified APK is
`48c95b35f4c874c2c9c93e48aca5575fa18c5859c86d4baed9f5e97a7daa7b81`.
Its full camera denial/retry/grant/public-review/resource-release acceptance
passes on fresh API 30 in 9.371 seconds. All 20 unsigned entries remain identical
after signing. Before/after source, artifacts and logs are retained in
`.cache/android-wallet/preview-cleanup-20260914/`. The isolated emulator uses
the qualified reaping adapter and adds no zombies to the original 26.

2026-09-14: composite balance/history rendering now conceals both views before
any clearing or formatting, then reveals them only after the entire update
succeeds. Initial clear failures and partial rendering errors attempt cleanup
of both views. Persistent platform refusal leaves both hidden, and the original
exception retains both cleanup failures. A later successful update restores
visibility. No snapshot, source authority, worker, timer or network is added.

Four public TextWatcher regressions fail on the old implementation, including
retained earlier text, visible partial data and masked primary errors. All four
pass on API 35/36 in 5.915/2.731 seconds. On API 30, those cases plus the existing
balance/history lifecycle suites pass together: ten tests in 9.553 seconds,
including repeated recreation past native registry capacity and cancellation of
old attempts. No wallet, key, endpoint or authentication is involved.

Android/JVM tests, debug/release/test builds, strict lint, fixture isolation,
architecture and native alignment pass. Six relevant native sync/JNI/history
ASan/UBSan/LSan groups pass in 0.17 seconds. The existing bounded sync-watch
fuzzer completes 68,125 cases in 121 seconds with no finding (4096-byte inputs,
five-second cases, 512 MiB RSS cap). Exact artifacts and before/after failure
logs are in `.cache/android-wallet/report-rendering-20260914/`.

The composite report adapter currently serves the debug public fixture; release
shrinking removes it while live balance networking remains disabled. The entire
unsigned release APK compares byte-identically to the preview-cleanup milestone,
so that milestone's exact minified APK and runtime acceptance remain applicable.
This is display failure acceptance, not network-source or custody qualification.
The reused isolated emulator exits cleanly through the qualified reaping wrapper
and leaves the original 26 ADB zombies unchanged.

2026-09-14: camera packing now has an independent test reference that enumerates
source occupancy and sampling strides instead of calling production helpers or
copying their ceiling-division calculation. It checks exact status, header,
every sampled pixel, length and the entire untouched output span. The existing
fuzzer uses it in place of its narrower length/canary checks. A new mandatory
unit group covers 1200 dimension/stride/padding layouts, capacity and malformed
metadata boundaries, and corruption of every header/pixel byte plus distant
tail positions. No production sampling, packet format or allocation changes.

All 88 final sanitizer groups pass in 63.40 seconds. Clang/GCC analysis and the
unchanged 10/15 complexity caps pass. Twelve production-sampler mutants and
four reference mutants fail intended assertions. Public QR fuzzing completes
4020 cases in 121 seconds; an additional wide-frame campaign completes 1656 in
61 seconds, including stride-two/three and capacity-refusal seeds. Neither
campaign finds a defect; both retain five-second case and 512 MiB RSS limits.

The exact release-archive sampling executable passes on x86-64 API 30/35/36;
ARM64 is compiled only. Its runtime SHA256 is
`bdaae04da7ab4ac0d2b461760e32fb1c942e62477669f3a1c803128eb3ef4fa6`.
Both ABI executables pass ELF protection/alignment inspection. Android/JVM,
build/lint, fixture isolation, native APK alignment and architecture checks pass.
The unsigned release APK remains byte-identical to the preview-cleanup
milestone, whose minified camera acceptance still applies. The full hazard
review is in [C_SAFETY_REVIEW.md](C_SAFETY_REVIEW.md); exact artifacts and evidence
are in `.cache/android-wallet/camera-sampling-20260914/`. Original ADB zombie
count remains 26, with their live parents preserved.

2026-09-14: request metadata now refuses U+2028 LINE SEPARATOR and U+2029
PARAGRAPH SEPARATOR, the two Unicode 17 Zl/Zp characters. Both were accepted
despite the existing refusal of controls and formatting characters, allowing
mandatory breaks inside an untrusted label/message. The production change
extends one existing table range; it adds no allocation, buffer or parsing
pass. Addresses, amounts, request bytes and Zclassic consensus remain unchanged.
This is a conservative display policy, not comprehensive spoof detection.
Classification and break behavior were checked against the pinned
[UnicodeData 17.0.0](https://www.unicode.org/Public/17.0.0/ucd/UnicodeData.txt)
and [Unicode 17 line-breaking specification](https://www.unicode.org/reports/tr14/tr14-55.html).

Before the fix, the native regression and independent fuzzer invariant fail;
actual Android QR/JNI decoding accepts all four separator/field combinations.
Afterward, all 88 ASan/UBSan/LSan groups pass in 63.63 seconds, with strict
Clang/GCC analyses and unchanged complexity caps. Six mutations fail intended
assertions, including removal of existing bidi checks and rejection of ordinary
Unicode neighbors. Bounded fuzzing completes 179701 cases in 121 seconds without
a finding (1024-byte input limit, five-second cases, 512 MiB RSS cap; observed
110 MiB). The exact release-archive native payment test passes on x86-64 API
30/35/36; ARM64 is compiled and inspected only.

Three real QR/JNI tests pass on each of API 30/35/36. Full Android/JVM tests,
debug/release/test builds, strict lint, fixture isolation, architecture and
16 KiB APK alignment pass. The new locally signed minified APK is
`4cb1c5b565d8df01cea5cbcab98a000a685022f8ea374994b2f56d70bb08e004`;
all unsigned ZIP entries retain their bytes after signing. Its full camera
denial/retry/grant/public-review/resource-release acceptance passes on fresh
API 30 in 9.385 seconds. The isolated emulator exits normally through the
qualified reaping adapter; the original 26 ADB zombies and their live parents
remain unchanged. Exact source, baseline failures, mutations, archives and
artifacts are in `.cache/android-wallet/request-separators-20260914/`.
The explicit hazard review is in [C_SAFETY_REVIEW.md](C_SAFETY_REVIEW.md).

2026-09-14: recovery displays and keyboard previews now conceal text before
clearing, transfer or rendering, and reveal it only after a complete update.
Android can retain its own text copy when a TextWatcher/platform operation
refuses clearing, even after the owned character array has been wiped. Both
direct clear/transfer failures and post-render failures now leave those copies
hidden. The keyboard remains usable and a successful later update restores its
preview. Existing bounds, ownership, zeroization and primary/cleanup failure
propagation remain intact; no additional worker, timer or secret copy is added.
This does not claim erasure of all framework/provider/GPU copies.

Five original device regressions fail at the visibility assertion after first
proving the public copied text survived and the owned array was cleared. A sixth
regression also proves ordinary text callbacks must see the view concealed;
all six fail on the old implementation. Final recovery-view suites pass all
18 tests on API 35/36 in 29.018/17.466 seconds. Fresh API 30 passes those plus
the actual MainActivity background-render-failure and worker-admission/retry
tests: 20 tests in 2.453 seconds. The background test now explicitly requires
the recovery preview to remain hidden after its failed clear. Public markers
only are used; positive hardware custody is not claimed.

Android/JVM tests, debug/release/test builds and strict lint pass, including
a final test build/lint after the activity assertion was added. Five relevant
native mnemonic, confirmation, custody-policy, secret-failure and JNI-key
ASan/UBSan/LSan groups pass. Bounded mnemonic fuzzing completes 1605946 cases
in 121 seconds without a finding (256-byte inputs, five-second cases, 512 MiB
RSS cap; observed 248 MiB). Fuzzer SHA256 is
`5d9686e46bcb5dbd5230b1b584e3dba1414e840cb6d1ed7f1db1ade2dd7ddb05`.
Both production ABI libraries remain byte-identical to the request-separator
milestone; no C or consensus logic changes.

Fixture isolation, architecture and 16 KiB APK native alignment pass. The
locally signed minified APK is
`53094c7275614bca7b0a6f57655d3b01988d00ad3cec7992cb60da0699fcfacb`;
every unsigned ZIP entry preserves its bytes after signing. Its full fresh
API 30 camera acceptance passes in 9.416 seconds; this is scanner/startup
regression evidence, not minified recovery or hardware-custody acceptance.
The isolated emulator exits normally through the qualified reaping wrapper;
the original 26 ADB zombies and their live parents remain unchanged. Baseline
failures, exact source/APKs and all evidence are retained in
`.cache/android-wallet/secret-concealment-20260914/`.

2026-09-14: unsigned-review text replacements now stay concealed until complete.
Expired/cancelled or partially rendered transaction text remains hidden if
Android refuses clearing. A normal later update restores visibility; malformed
formatting retains the existing unavailable-state behavior. The inline wrapper
adds no closure allocation, owner, timer or authority. Original and cleanup
exceptions remain available for diagnosis.

Four public regressions fail before the fix, including real C expiry through
ReviewPresentation: the native owner closes and its single slot is returned,
but the framework still holds visible transaction details. Final view and
lifecycle suites pass all 14 cases on API 30/36 in 6.285/119.486 seconds. The
API 35 combined host command reaches its 180-second capture timeout during
the last case; the device TestRunner subsequently reports 14 tests, zero failed
and zero ignored. No job is hung or emulator terminated. Two smaller reruns
capture clean command completion: four lifecycle cases in 122.583 seconds and
ten view cases in 14.626 seconds, with every assertion/deadline unchanged.
Recreation, background rejection, explicit close, queued updates, formatting,
failure visibility and native slot reuse are covered.

Android/JVM, debug/release/test builds, strict lint, fixture isolation, native
alignment and architecture checks pass. Four relevant native review/JNI/context/
sighash sanitizer groups pass in 0.13 seconds. The independent existing review
state fuzzer completes 325099 executions in 121 seconds without a finding
(640-byte inputs, five-second cases, 512 MiB RSS cap; observed 243 MiB).
Its SHA256 is `50a6e0c82773272d7a87b5276ceb2f564505bb7e2a04f7853425cceaf950f676`.

The unsigned-review UI remains disabled in the launcher and is removed by
release shrinking. The entire unsigned release APK is byte-identical to the
secret-concealment milestone; its exact minified artifact/runtime evidence still
applies. This is debug public-fixture display acceptance, not authenticated
funding, consent, signing or broadcast qualification. The reused isolated API 30
emulator exits normally through the qualified reaping wrapper, leaving the
original 26 ADB zombies and their live parents unchanged. Exact source, baseline
failures, timeout evidence, retries and artifacts remain in
`.cache/android-wallet/review-concealment-20260914/`.

2026-09-14: payment metadata fuzzing now uses an independent UTF-8 reference
derived from Unicode 17's byte-sequence table and RFC 3629's grammar. It reuses
the independently verified full Cf enumeration, with Cc and line/paragraph
separators, instead of calling production decoding/classification helpers. The
new property covers all successful decoded labels/messages and compares raw
fuzzer inputs directly; it subsumes the former separator-only assertion.

A mandatory unit checks every Unicode codepoint position, every overlong form
below U+10000, all four-byte patterns above the scalar ceiling, all first/second
byte pairs at lengths 1..4, and full/truncated field boundaries: 2427023 bounded
comparison cases. Production decoding and allocation are unchanged. Twelve
deliberate production errors and six reference errors all fail intended
assertions. All 89 sanitizer groups pass in 64.34 seconds, with Clang/GCC
analysis and unchanged 10/15 complexity caps. Expanded payment fuzzing passes
181911 executions in 121 seconds without a finding (five-second cases,
1024-byte inputs, 512 MiB RSS cap; observed 109 MiB).

The exact release-archive executable passes on x86-64 API 30/35/36;
ARM64 is compiled only. Runtime SHA256 is
`ec927faba469e48505baae04d8b1a3a1a92019230064c2b581a4db8a7ef8ecc6`.
Both ABI executables pass ELF protection/alignment inspection. Android/JVM,
build/lint, fixture isolation, native alignment and architecture gates pass.
Debug, release and test APKs all remain byte-identical to the prior milestone,
whose exact device acceptance remains applicable. No new emulator is required;
the original 26 ADB zombies and live parents remain unchanged. Standards,
source, mutation/fuzz evidence and artifacts are in
`.cache/android-wallet/utf8-reference-20260914/`. The full hazard review and
primary source links are in [C_SAFETY_REVIEW.md](C_SAFETY_REVIEW.md).

2026-09-15: completed the pending camera canvas cleanup. Preview drawing now
restores the caller's save depth, transform and clip even when Android rejects
the bitmap draw. The existing failure fixture uses a recycled public bitmap in
an unattached view; its recorded baseline fails because the save depth grows
from two to three. A successful-draw fixture also checks all four sensor
orientations and both facing modes, public pixels and untouched caller state.
This does not qualify physical camera orientation or hardware custody.

All six preview regressions pass on API 30/35/36 in 0.977/12.920/4.235 seconds.
API 35/36 results and the seven relevant native sanitizer groups are preserved
in `.cache/android-wallet/canvas-cleanup-20260914/`; API 30 and current-build
evidence are in `.cache/android-wallet/resume-20260915/canvas/`. Current Android
and JVM builds/tests, debug/release lint, fixture isolation and 16 KiB native
alignment pass. The unsigned release APK matches the saved candidate exactly,
SHA256 `16e232b8684a6b924833251d2b1ab61d680b73cc81a0a683b67491874c7fbeda`.
Its locally signed minified APK has SHA256
`66f4cc487d8e0de0317428b81f96a794a578b980533cfc5f4435af22e72391d5` and passes
the full public permission-denial/retry/grant/camera-review/cleanup fixture on
the isolated API 30 profile in 9.318 seconds. No C, wallet or consensus behavior
changes. The owned emulator uses the qualified reaping adapter and exits by
normal console shutdown; the pre-existing emulator profiles remain intact.

2026-09-15: the native RNG regression now observes the complete scratch wipe
while the buffer is still alive. It requires one full wipe on every admitted
success/failure, exact remaining spans and nonblocking OS requests, and no OS
or wipe calls on invalid arguments. Every width 1..64 is tested at both sides
of the 128-attempt limit. A partial secret followed by 127 interruptions must
be erased while caller output remains unchanged. No production C or API changes.

The old fixture accepts a missing-wipe mutation; the new one rejects it and
nine other deliberate defects with intended assertions. An initial test-helper
complexity violation is preserved and fixed by extracting the interruption
predicate, retaining the 10/15 caps. All 89 sanitizer groups pass in 64.28 seconds;
Clang/GCC analysis, strict warnings and final focused checks pass. The exact
release-archive executable passes on x86-64 API 30/35/36; ARM64 is compiled only.
Android/JVM/build/lint, fixture isolation and 16 KiB native alignment pass. The
release APK remains byte-identical to the canvas checkpoint. Full hazard review
is in [C_SAFETY_REVIEW.md](C_SAFETY_REVIEW.md); exact evidence and mutations are
in `.cache/android-wallet/resume-20260915/random/`.

2026-09-15: the JNI phrase adapter now returns NULL when an allocation callback
supplies an array with an exception pending. Its native return contract now
matches the shared byte-array adapter; pending exceptions and secret cleanup
are preserved. This is a reproduced fake-VM inconsistency, not an observed
Android runtime disclosure or exception bypass. The regression covers all six
array-producing key/header entries and verifies no copied output or further
ordinary VM operation is published on refusal.

Both the native regression and seeded fuzzer fail on the preserved old source.
All 89 corrected sanitizer groups pass in 64.56 seconds, with Clang/GCC analysis
of production and unit/fuzzer modes and unchanged 10/15 complexity caps. Bounded
fuzzing completes 52726 cases in 121 seconds without a new finding. Android/JVM,
build/lint, fixture isolation and native alignment pass. The public-vector JNI
test passes on API 30/35/36 in 0.334/2.147/0.700 seconds. Initial API 35 command
delivery was delayed but completed within the original timeout; no emulator
restart or timeout relaxation was needed. Hardware custody remains unqualified.
The explicit review is in [C_SAFETY_REVIEW.md](C_SAFETY_REVIEW.md); baseline
assertions, source, fuzz corpus and APK identities are preserved in
`.cache/android-wallet/resume-20260915/jni-phrase/`.

2026-09-15: camera JNI now allocates and clears the exact packet size from a
shared bounded C geometry calculation. A measured 640x480 input requests 76805
native scratch bytes instead of 147461: 70656 fewer, about 48%. This is buffer
allocation/erasure evidence, not a claim about whole-process RSS or CPU time.
Sampling and packet bytes remain unchanged. Invalid sampled dimensions refuse
before VM buffer access or allocation, and all failure paths still clear the
entire owned allocation.

The old allocation regression fails with both byte counts recorded. Eight
dimension cases, all 1200 independent sampling layouts and three deliberate
mutations pass their acceptance checks. All 89 sanitizer groups pass in 64.98
seconds; strict Clang/GCC production/fixture analysis and unchanged complexity
caps pass. Seeded camera fuzzing completes 3231 cases in 121 seconds without a
finding (five-second cases, 512 MiB RSS cap; observed 258 MiB). The JNI allocation
fixture linked against release archives passes on x86-64 API 30/35/36; ARM64
is compiled only. Its NDK/OpenJDK JNI table-tag adaptation is test-only.

Android/JVM/build/lint, fixture isolation and native alignment pass. The locally
signed minified APK is
`ed509f52527502c8278e3e2e303da95cebe129dcf2558eb88f99d3b6359a50fb`, with all
unsigned entry bytes preserved. Full permission-denial/retry/grant/exact-review/
cleanup acceptance passes on fresh API 30 in 9.379 seconds. A prior reused-profile
run failed to locate the permission Deny control before capture; the visible
dialog, logs and state are retained without claiming a root cause. The same
test and deadlines pass on the fresh profile. Owned emulators use the qualified
reaping wrapper and normal shutdown. Full review is in
[C_SAFETY_REVIEW.md](C_SAFETY_REVIEW.md); exact artifacts and all evidence are
in `.cache/android-wallet/resume-20260915/camera-allocation/`.

2026-09-15: the complete 108-commit Android development checkpoint at
`9c0e6671789f93cf0970e7cbe4daa3ec52be85ec` is preserved in the user-authorized
private `CesareFI/zclassic-android-wallet` repository, branch
`agent/android-wallet-20260911`. Direct Git advertisement and GitHub's reference
API both match local HEAD; the saved commit lists match, ahead/behind is 0/0,
and the working tree was clean. `wallet-backup` is the development upstream;
`origin` remains `https://github.com/z23c/z23.git` for fetching. Cache, APK,
emulator, credential and wallet-state artifacts are excluded. Existing Gradle
bootstrap and documented public unsigned transaction fixtures remain tracked
inputs. The main-only hook required a one-command exception for this private
backup after the wallet gates and exact source hashes passed; installed hooks
and source history were unchanged. This records a source checkpoint, with no
main integration, deployment or new custody claim. Evidence is in
`.cache/android-wallet/resume-20260915/wallet-backup-checkpoint/`.

2026-09-15: BIP39 seed derivation now prepares its HMAC-SHA512 key pads once
per call and clones the vendored provider state for each round. One-shot HMAC
shares the same bounded implementation. BIP39 still performs exactly 2048 rounds
and publishes the same 64-byte seed; all prepared state, intermediate digests
and accumulators are cleared before returning. No heap allocation is added.

The public-vector benchmark is retained as `native/tests/bench_mnemonic_seed.c`
and explicit host target `bench_mnemonic_seed`. It checks every result while
timing five batches of 200 derivations after warm-up. Against exact release
archives on the API 30 x86-64 emulator, median thread CPU time changed from
20.523340 to 11.640076 ms for the 93-byte/12-word vector (43.28% lower), and
30.425060 to 11.726748 ms for the 187-byte/24-word vector (61.46% lower). These
are emulator CPU measurements, not physical-device latency or battery claims.
The benchmarked archive matches the archive subsequently packaged and tested.
The measured GCC-O2 KDF frame is 704 bytes; the one-shot HMAC frame grows from
288 to 480 bytes. These are per-function frames, not whole-call-stack or RSS
measurements. The unsigned release APK grows by 672 bytes to 612903 bytes.

All 89 sanitizer groups pass in 48.93 seconds. Clang/GCC analysis, strict warnings,
10/15 complexity caps and the architecture placement gate pass. The standalone
benchmark also passes its sanitizer execution and analysis. Independent OpenSSL
checks cover 63 key/salt boundary combinations and 96 complete receive/change
derivations. Provider fault tests cover every preparation step and every digest
step in five selected rounds, including the final round, for short/long keys.
They require at most three live SHA512 contexts, full erasure before release,
exact scratch wipe counts, preserved caller inputs and unchanged output on
failure even when a provider partially writes its output. All 13 deliberate
cleanup, publication, round-count, accumulation, pad and normalization defects
are rejected by their intended assertions.

The new optional host `fuzz_mnemonic_seed` target requires `ZCL_FUZZ=ON`,
`ZCL_SANITIZE=ON` and `ZCL_ORACLE=ON`; it compares the public mnemonic/ASCII
passphrase path against OpenSSL and checks refusal of short output capacities.
It completes 7842 cases in 121 seconds without a finding, with a five-second
case limit and 512 MiB RSS cap (403 MiB observed). Release-archive HMAC, all
24 published mnemonic vectors and cleanup/failure fixtures pass on x86-64
API 30/35/36; ARM64 is compiled only. The public-vector JNI test passes on
API 30/35/36 in 0.306/1.200/0.588 seconds. Android/JVM builds and tests,
debug/release lint, fixture isolation and native alignment pass. The unsigned
release APK SHA256 is
`8d88ec9944e4ac00e2ba3222e17f53ac2725e2487680ef6da7128688c5847d4d`.
TLS remains quarantined and hardware-positive custody remains unqualified.
The full hazard review is in [C_SAFETY_REVIEW.md](C_SAFETY_REVIEW.md); exact
sources, archives, measurements, failure logs and mutations are preserved in
`.cache/android-wallet/resume-20260915/seed-measure/`.

2026-09-15 11:22 UTC: the prepared-HMAC seed milestone is remotely checkpointed
at `24a6886841cf0711d19967022d99fcbf4b9228d1`. Local HEAD, direct Git
advertisement and GitHub's reference API agree. The private repository retains
the original 108-commit checkpoint as an ancestor, plus this milestone; the
worktree was clean with 0/0 ahead/behind and the intended `wallet-backup`
development upstream. `origin` remains unchanged. Verified receipt and commit
lists remain in the ignored `wallet-backup-checkpoint/` evidence directory.

2026-09-15: native EC context lifetime now has an independent failure fixture.
The existing key-failure test accepted a deliberate removal of provider-context
destruction; the implementation already contained the correct cleanup. The new
test observes destroy-before-wipe-before-free, exact full-allocation erasure,
point/encoding scratch erasure and cleared-owner reuse. It also rejects partial
provider output and incorrect successful output lengths while preserving caller
inputs and output guards. Context-size cases include 0, 1024, 1025 and SIZE_MAX;
allocation, construction, blinding, point and serialization refusals are covered.

All 90 sanitizer groups pass in 49.08 seconds. The final fixture passes focused
sanitizers after strengthening two provider-stop assertions, Clang/GCC analysis,
strict warnings and the unchanged 10/15 complexity caps. Architecture placement
passes. Its measured GCC-O2 maximum frame is 224 bytes. An unchanged mutation
control passes; ten deliberate destruction, erasure, provider-result and length
defects fail their intended assertions. The exact fixture linked against the
existing release archives passes on x86-64 API 30/35/36; ARM64 is compile-only.
The x86 fixture SHA256 is
`b8b815c74147e99077e49d084d2d45aa3b1187d05c9280d4bf82f0eda1c76f2f`.
This adds regression evidence without changing production code or claiming
hardware custody. Test inputs use the public scalar-one generator, fixed fixture
blinding and owned temporary executables. No APK, provider, JNI or TLS source is
changed. The full review is in [C_SAFETY_REVIEW.md](C_SAFETY_REVIEW.md); old-test
acceptance, mutations, hashes, analysis and device evidence are preserved in
`.cache/android-wallet/resume-20260915/ec-lifetime/`.

2026-09-15 11:33 UTC: EC lifetime regression commit
`d200dfa09146746eb1383e200c0f8ae0f30e3c9a` is verified on the private
`wallet-backup` development branch. Local/tracking/direct-Git/GitHub-API SHAs
agree, the tree was clean with 0/0 ahead/behind, and all 110 wallet commits
remain preserved. The original 108-commit checkpoint is still an ancestor.
`origin` is unchanged; only the same-named development branch was pushed.

2026-09-15: BIP32 now has an optional host differential fuzzer, using a shared
OpenSSL reference extracted from the existing receive/change oracle. The
reference independently computes master and child private-key/chain-code bytes,
validates scalar bounds and derives compressed public keys. Existing public
vectors and the fuzzer reuse that reference; no second wallet implementation
or dependency enters the APK.

`wallet_bip32_differential` replays 860 deterministic public cases and can write
them to a fresh corpus directory through exclusive file creation. Cases cover
seed lengths 0..66/SIZE_MAX; normal/hardened index boundaries; scalar zero, one,
order-1, order, order+1 and all-ones; NULL arguments and invalid blinding lengths.
The fuzzer explores up to five child steps, arbitrary parent/chain-code bytes,
output guards, both derived fields, unchanged inputs on refusal and public-key
independence from two fixture blinding values. It accepts at most 104 input bytes
and uses no real wallet material, entropy source, filesystem or network itself.

All 90 default sanitizer groups pass in 48.93 seconds. In both oracle-only and
oracle/fuzz builds, the final 860-case replay, all 17 published BIP32 paths and
all 96 receive/change comparisons pass (three focused groups, 4.21/4.65 seconds).
The exact fuzzer completes 37985 cases in 121 seconds without a finding, with
five-second case and 512 MiB RSS bounds (264 MiB observed). Its initial corpus
contains 852 deterministic cases; eight additional exact scalar-boundary cases
were then qualified by the final replay against the same unchanged harness.
Nine wallet-derivation mutations fail the intended differential assertion.
Two oracle scalar-admission mutations fail specifically in the added boundary
cases. An earlier mutation of the entire oracle curve order failed sooner on
ordinary derivations; its log is retained separately.

Clang/GCC analysis of the reference, harness, driver and both fixed-vector modes
passes, together with strict warnings, unchanged 10/15 complexity caps and
architecture placement. The largest measured GCC-O2 harness frame is 880 bytes;
the optional corpus writer uses 1104 bytes. Android/JVM builds/tests, lint,
fixture isolation and native alignment pass. The unsigned release APK remains
byte-identical at SHA256
`8d88ec9944e4ac00e2ba3222e17f53ac2725e2487680ef6da7128688c5847d4d`.
No production native/JNI/provider implementation changes. TLS stays quarantined;
this host-only milestone grants no hardware-custody or physical-device claim.
Review: [C_SAFETY_REVIEW.md](C_SAFETY_REVIEW.md). Exact sources, binary hashes,
corpus, mutation controls and validation logs remain in
`.cache/android-wallet/resume-20260915/bip32-fuzz/`.

2026-09-15 11:56 UTC: BIP32 differential commit
`b25ca47f81fc5906655f87ed3f0373aaf48e1035` is verified on the private
`wallet-backup` development branch. Local/tracking/direct-Git/GitHub-API SHAs
agree; the tree was clean with 0/0 ahead/behind and all 111 wallet commits
preserved. `origin` remains unchanged. `NEXT_MILESTONE.md` now reflects the
implemented change reservation/recovery, live review, branch/sighash, signature
and public signed-wire primitives, with links to their existing evidence.
Authenticated current-chain context, per-use custody, consent, completion-time
checks and broadcast acceptance remain open. This is a continuation correction;
no implementation or acceptance gate changes.

2026-09-15 11:59 UTC: continuation correction commit
`0ad6900cdce03303f3353b9975a14ada403210de` is verified on the private
`wallet-backup` development branch. Local/tracking/direct-Git/GitHub-API SHAs
agree, with a clean tree, correct upstream and 0/0 ahead/behind. All 112 wallet
commits remain preserved; `origin` remains unchanged.

2026-09-15: recovered change now derives its BIP39 seed once per invocation,
reusing it only after the exact record anchor matches. It still uses two
independently blinded curve contexts with at most one live allocation. The
complete local work object is explicitly wiped on success and every admitted
failure. The extracted seed/address helpers are private native interfaces;
public APIs, address paths, record bytes and authenticated-caller requirements
remain unchanged. There is no seed cache or new transaction/custody authority.

The public-vector benchmark uses exact release archives and independent OpenSSL
expected addresses. On API30 x86-64, five 100-call samples after ten warmups per
fixture reduce median thread CPU from 25.510225 to 13.485252 ms for 16-byte
entropy (47.14%) and 25.027664 to 13.431868 ms for 32-byte entropy (46.33%).
Every result is checked. The recovered-change frame measures 352 bytes versus
288 before; there is no new allocation, and all authored frames remain within
the 4096-byte limit. These emulator results do not establish physical-device
performance or battery use. The explicit host benchmark has no timing pass bar.

All 90 default sanitizer groups pass in 49.04 seconds. Production and fixture
Clang/GCC analysis, strict warnings, architecture and complexity caps 10/15 pass.
The independent oracle adds 48 recovered-change bindings to its existing
96 receive/change comparisons. Failure tests check private helper bounds,
allocation/construction/blinding failures in either context, all ten child steps,
provider errors throughout both paths, mismatched anchors, short provider output,
unchanged output guards and exact live seed cleanup. Three mutation controls pass;
ten deliberate erasure, KDF-reuse, blinding, path, binding and publication defects
fail their intended assertions. Fuzzing completes 3905 cases in 121 seconds under
five-second case and 512 MiB RSS bounds (48 MiB observed), without a finding.

Android/JVM builds/tests, debug/release lint, fixture isolation and native
alignment pass. Exact release-linked recovery, key-failure and secret-failure
fixtures pass on x86-64 API30/35/36; ARM64 remains compile-only. The release APK
is 613047 bytes (+144), SHA256
`a716ced30ebf97eef777657edf229b79fadf350d45e76f6c0f667df911429ba2`.
The packaged archive and committed benchmark match the measured candidate.
TLS remains quarantined and hardware-positive custody remains unqualified.
Full hazard review: [C_SAFETY_REVIEW.md](C_SAFETY_REVIEW.md). Exact measurements,
reference derivation, mutation controls, initial diagnostic failures, hashes and
acceptance logs are preserved in
`.cache/android-wallet/resume-20260915/change-seed-measure/`.
The public-vector JNI mnemonic/refusal/receive test also passes on API30/35/36
in 0.422/1.410/0.635 seconds. It exercises the real VM/native path without
creating fresh entropy, persisting a wallet or invoking hardware-authentication
fallbacks. All temporary native fixture executables were removed after success.

2026-09-15: recovered-change seed reuse commit
`22312b55101cafa3719bd4a193099008d05c9d2b` is remotely verified on the private
`wallet-backup` development branch. Local/tracking/direct-Git/GitHub-API SHAs
agree, with a clean tree, correct upstream and 0/0 ahead/behind. All 113 wallet
commits remain preserved, including the initial 108-commit checkpoint. `origin`
remains unchanged; only the development branch was pushed.

2026-09-15: secret JNI output now has a managed cleanup owner before native
execution. The old fault model copied public secret bytes into a native-created
Java array and then raised a VM exception, losing the return reference while
still passing its native-scratch checks. The preserved reproduction establishes
that ownership gap; it does not claim an observed real-device leak.

`WalletKeys` now allocates a fixed destination before calling the private JNI
entry. C returns a checked length and clears its own scratch. Managed finally
clears the destination after any refusal/exception and after extracting a shorter
prefix; exact-capacity success transfers that same owned array. Public methods,
lengths, errors and BIP39 behavior stay unchanged. The private JNI signatures
change together with their only managed caller. No new key cache, provider
fallback, authentication or transaction authority is introduced.

All 90 default sanitizer groups pass in 49.78 seconds, followed by final focused
native checks after a test-only NDK/JDK table-name portability correction.
Clang/GCC production/fixture analysis, strict warnings, architecture and unchanged
10/15 complexity caps pass. JNI fuzzing completes 125794 cases in 121 seconds,
max_len217, timeout5 and RSS cap512 MiB (68 MiB observed), without a finding.
Six native and four managed mutations fail intended cleanup/bounds/exception
assertions; their unmodified controls pass. Managed tests cover bounded capacities,
every shorter prefix, complete scratch cleanup and original error identity.

A separate host test library forwards the actual production JNI code's array
operations to the real JVM, copies a selected public prefix and raises a supplied
OutOfMemoryError. The tests observe the copied prefix before rethrowing and the
same complete array erased after managed finally, with the same Throwable.
Ten such prefix cases pass under -Xcheck:jni. The bridge is excluded from Android
builds, and APK library inventory/export checks confirm its absence. This proves
cleanup for an injected exception, not actual memory-exhaustion behavior or
erasure of every VM/provider/UI copy.

Android/JVM builds/tests, debug/release lint, fixture isolation and native
alignment pass. The unsigned release APK is 613351 bytes, SHA256
`5df0e14b60a6f81f637bf556e76806cf49ddc3361ecd996680f6668d07281da3`.
A shorter secret result adds at most one bounded temporary managed array, which
is explicitly cleared; full-capacity results reuse their sole destination. There
is no RAM-saving claim for this custody improvement. TLS remains quarantined;
hardware-positive custody remains unqualified. Full review:
[C_SAFETY_REVIEW.md](C_SAFETY_REVIEW.md). Reproduction, exact sources/hashes,
mutations, initial diagnostics and acceptance evidence remain in
`.cache/android-wallet/resume-20260915/jni-secret-output/`.
The public JNI vector/refusal test and the new all-five-entropy-length test pass
on API30/35/36 in 0.839/3.756/0.897 seconds. The exact release-core-linked
native exception fixture also passes on all three x86-64 emulators; ARM64 is
compile-only. Its x86 executable SHA256 is
`30e38edaa36365798f14efd67c3d38f757f8ffda337c09315bebe134410bf974`.
Owned device executables were removed. No fresh device entropy-generation test
was invoked. The final host fuzzer rebuild replays its complete retained corpus
successfully after the test table-type portability adjustment.

2026-09-15: JNI secret-output ownership commit
`07b26ec0c1348532b53b776b00314847947047a8` is remotely verified on the private
`wallet-backup` development branch. Local/tracking/direct-Git/GitHub-API SHAs
agree, with a clean tree, correct upstream and 0/0 ahead/behind. All 114 wallet
commits remain preserved; `origin` remains unchanged. Only the same-named
private development branch was pushed.

2026-09-15: authenticated change-state fuzzing now optionally compares complete
records against the existing independent OpenSSL HKDF/HMAC backend. That backend
is extracted into a shared host-only fixture, with checked spans and atomic
output on failure; the fixed-vector driver reuses it. The original 40 combinations
remain, augmented to 120 with nonuniform counter patterns and both implementations'
output guards. Its oracle treats the wallet header as public context, without
requalifying address derivation, hardware/GCM authentication or record freshness.

On the same public input, the old round-trip harness accepts six isolated defects
that the oracle-enabled harness rejects: changed HKDF salt/label/block/context,
changed record profile and a counter byte-order change shared by encoder and
decoder. Both unchanged controls pass. This shows why matching one implementation's
own encode/decode is insufficient to establish exact serialized authentication.
Production code, provider dependencies, record format and all custody gates remain
unchanged; no new cryptographic implementation enters the app.

All 90 default sanitizer groups pass in 48.56 seconds. Oracle-only and fuzz/oracle
profiles pass all 120 comparisons and new oracle bounds in 1.57/2.01 seconds.
Clang/GCC analysis, strict warnings, architecture and unchanged complexity caps
10/15 pass. The oracle-enabled fuzzer completes 3778 cases in 121 seconds without
a finding, with max_len118, timeout5 and RSS cap512 MiB (97 MiB observed).
Maximum measured authored GCC-O2 frames are 352/688/576 bytes for the reference,
harness and fixed driver. No whole-stack or production-memory claim is implied.

Android/JVM builds/tests, debug/release lint, fixture isolation and native
alignment pass. All three APKs are byte-identical to the accepted JNI-owner
milestone; release SHA256 remains
`5df0e14b60a6f81f637bf556e76806cf49ddc3361ecd996680f6668d07281da3`.
TLS remains quarantined; hardware-positive custody remains unqualified. Full
review: [C_SAFETY_REVIEW.md](C_SAFETY_REVIEW.md). Exact source/binary identities,
old-harness mutation acceptance, new differential failures, corpus and validation
are preserved in `.cache/android-wallet/resume-20260915/change-state-fuzz/`.

2026-09-15: change-state differential checkpoint
`264b10793cd45d8b792bfd8e178d9f1c4ce08239` is verified on the private
`wallet-backup` development branch. Local/tracking/direct-Git/GitHub-API SHAs
agree; the tree was clean, upstream correct and ahead/behind 0/0. All 115 wallet
commits remain preserved, including the original 108-commit checkpoint. Origin
remains `https://github.com/z23c/z23.git`, used only for upstream fetching.

The Android decryption handoff was inspected before further custody edits.
`unlockAfterAuthentication` already clears its returned plaintext in finally.
The inspected [AOSP Keystore implementation](https://raw.githubusercontent.com/aosp-mirror/platform_frameworks_base/refs/heads/main/keystore/java/android/security/keystore2/AndroidKeyStoreCipherSpiBase.java)
first obtains an internal plaintext array for the caller-buffer overload, then
copies it into the destination. Switching overloads would add a secret copy
without resolving provider-internal cleanup. The existing call is retained;
no provider leak or additional hardware-custody qualification is claimed.

2026-09-15: direct HMAC-SHA512 differential fuzzing now compares arbitrary bounded
public key/message bytes with OpenSSL3. Each admitted case exercises a valid
full digest before invalid arguments, and checks output guards, unchanged inputs
and refusal atomicity. Its deterministic driver covers298 boundary/refusal cases,
including empty inputs, SHA512 block/padding boundaries and maximum256-byte keys
and512-byte messages. Both targets are host-only under ZCL_ORACLE, with fuzzing
additionally requiring the existing sanitized ZCL_FUZZ profile. Production code,
cryptography, custody and APK dependencies are unchanged.

Six isolated boundary defects pass the earlier fixed HMAC/PBKDF2 executable and
fail the new digest comparison. Both unchanged controls pass. The direct fuzzer
completes2278672 cases in121 seconds without a finding, with max_len774, timeout5
and RSS cap512 MiB (281 MiB observed). All90 default sanitizer groups pass
in48.54 seconds; oracle-only HMAC/differential checks pass in0.73 seconds and the
fuzz-profile deterministic driver passes. Clang/GCC analysis, strict warnings,
architecture and unchanged complexity caps10/15 pass. Maximum measured authored
GCC-O2 frames are1824 bytes for the harness and1104 for its corpus writer.

Android/JVM builds/tests, debug/release lint, fixture isolation and native alignment
pass. All three APKs remain byte-identical; release SHA256 remains
`5df0e14b60a6f81f637bf556e76806cf49ddc3361ecd996680f6668d07281da3`.
No new hardware-custody or production-device behavior is claimed; TLS stays
quarantined. Review: [C_SAFETY_REVIEW.md](C_SAFETY_REVIEW.md). Public mutation,
fuzz, source and validation evidence is preserved in
`.cache/android-wallet/resume-20260915/hmac-fuzz/`.

2026-09-15: direct HMAC differential checkpoint
`19c5159e484ada9533ec99bfab225e0ec32acefa` is verified on the private
`wallet-backup` development branch. Local/tracking/direct-Git/GitHub-API SHAs
agree; the tree was clean, upstream correct and ahead/behind 0/0. All 116 wallet
commits remain preserved, including the original 108-commit checkpoint. Origin
remains unchanged; private backup permissions report push:true/admin:true.

2026-09-15: the host fuzz build now instruments both secp256k1 targets for
coverage feedback. They already had ASan/UBSan; wallet core and the other enabled
providers already had coverage instrumentation. The old gate passed without the
curve coverage flags. The strengthened gate checks the actual emitted commands:
sanitisers/fail-on-finding on 151 authored/provider compilations and coverage on
102 library/fuzz compilations, while standalone fault-test copies retain their
sanitizer checks. Twelve manifest mutations and a separate removed-assertion
mutation validate refusal. Rebuilt curve object symbols independently confirm
coverage hooks that were absent from the baseline.

All 94 tests in the complete fuzz/oracle build pass in 114.54 seconds; all 90
default sanitizer groups pass in 54.65 seconds. The strengthened profile and its
12 manifest mutations pass in 8.13 seconds. BIP32/signature oracle checks pass,
and a bounded BIP32 campaign completes 8,367 cases in 121 seconds without a
finding (max_len104, timeout5, RSS cap512 MiB, observed264 MiB). Strict native
analysis, architecture and unchanged complexity caps10/15 pass. Android/JVM
builds/tests, debug/release lint, fixture isolation and alignment pass; all three
APKs remain byte-identical to the prior checkpoint. Production C, cryptography,
Android options and custody gates are unchanged. TLS remains quarantined.

Review: [C_SAFETY_REVIEW.md](C_SAFETY_REVIEW.md). Initial scope/log-format
corrections, exact old/new commands and symbols, manifest mutations, public corpus
and acceptance are preserved in
`.cache/android-wallet/resume-20260915/fuzz-coverage/`.

2026-09-15: native fuzz coverage checkpoint
`44df891d7056546264457d4c2a0852e2de911684` is verified on the private
`wallet-backup` development branch. Local/tracking/direct-Git/GitHub-API SHAs
agree; the tree was clean, upstream correct and ahead/behind 0/0. All 117 wallet
commits remain preserved, including the original 108-commit checkpoint. Origin
remains unchanged; only the same-named development branch was pushed.

2026-09-15: backup-screen construction now owns incoming words from entry and
clears/conceals the screen after any construction failure, including fatal errors.
Previously, a failure while adding an action button after displaying words could
leave the view visible: the phrase-delivery owner cleared its character array,
but did not clear or conceal the screen. An API30 public-marker fixture reproduced
that incomplete visible screen before the fix. The screen method now clears the
incoming array, attempts view cleanup and preserves the original error with any
cleanup failure attached. A successful construction retains the existing view
ownership and lifecycle behavior; no extra worker, buffer or secret copy is added.

Four instrumented regressions cover failure before view ownership, either action
button, and a separately retained framework buffer whose cleanup also throws.
The latter attaches its watcher before rendering; an initial fixture attached it
after rendering and incorrectly expected a separate copy even though Android was
still using the already-wiped character array. That failed assumption and the
corrected fixture are preserved. Assertions still require hidden failed output,
exact ownership cleanup, error identity, cleanup-error preservation and a usable
subsequent screen. Only public marker text and the existing storage-free debug
host are used; no authentication, wallet creation or real seed is involved.

The four new checks plus the existing secret-view and concealment checks pass
as 22-test suites on x86-64 API30/API35/API36 emulators in
53.857/121.338/122.097 seconds. These exercise the actual Android debug views
with injected errors; they do not prove global-OOM behavior, physical-device
custody or erasure of every framework/GPU copy. Existing emulator profiles are
preserved. Android/JVM builds/tests, debug/release lint, fixture isolation,
architecture and 16 KiB native alignment pass. C/JNI/providers are unchanged,
and both debug native libraries are byte-identical to their baseline.

The unsigned release APK is 613351 bytes, SHA256
`0a0de7c5b92c04a426f5351f2b14116f35661a6afb71f466754fd445d80f0311`.
The instrumented test uses the existing nonexported debug host, which remains
absent from release. No minified backup-screen runtime or positive hardware
custody qualification is claimed. TLS remains quarantined. Exact source/APK
identities, the failing baseline, initial/corrected test fixtures and validation
are preserved in `.cache/android-wallet/resume-20260915/backup-screen/`.

2026-09-15: backup-screen checkpoint
`b6d68e57cfbb500ed2adb5d65c947862ba0ee040` is verified on the private
`wallet-backup` development branch. Local/tracking/direct-Git/GitHub-API SHAs
agree; the tree was clean, upstream correct and ahead/behind 0/0. All 118 wallet
commits remain preserved, including the original 108-commit checkpoint. Origin
remains unchanged; only the same-named development branch was pushed.

2026-09-15: host Clang sanitizer builds now check unsigned integer overflow and
implicit integer truncation/sign changes throughout authored native C, including
JNI and fault-test copies. Ordinary UBSan does not include these checks. An
isolated audit of the unchanged checkpoint first passed all 94 baseline tests
in 70.92 seconds. No production arithmetic defect was found or suppressed.
Provider arithmetic retains ASan/UBSan; GCC retains the existing sanitizer
profile, and Android compilation is unaffected.

Three process-isolated compiler probes verify that the extra instrumentation
stops faults accepted by an ordinary-UBSan control. The checker requires both
a numeric failure exit and the expected diagnostic; timeout/signal outcomes do
not qualify. Cache-only mutations verify rejection of disabled instrumentation,
recovering diagnostics, unrelated failure and timeout with a misleading expected
diagnostic. The fuzz compile-command gate now also requires the added checks;
all 13 manifest mutations are refused. Independent inspection confirms the extra
flags on all 133 authored compilations in the promoted fuzz build, while its
18 provider compilations retain their existing sanitizer profile.

All 95 fuzz/oracle test groups pass in 113.34 seconds, and all 91 default native
groups pass in 57.45 seconds. The runtime probe check passes again after tightening
timeout rejection. Bounded campaigns complete 3,193,474 amount cases and 101,189
synchronization cases in 121 seconds each without a finding. Limits are 120 seconds
per campaign, five seconds per case and 512 MiB RSS; observed peaks are 257/93 MiB.
Amount max_len is 64; synchronization permits 16385 bytes, though this short run
only grew its mutation length limit to 205 bytes. Existing deterministic boundary
tests remain required; this run does not claim exhaustive input coverage.

Strict Clang/GCC compilation and analysis, unchanged complexity caps 10/15,
architecture, Android/JVM builds/tests, debug/release lint, fixture isolation and
native alignment pass. All three APKs are byte-identical to the backup-screen
checkpoint; release SHA256 remains
`0a0de7c5b92c04a426f5351f2b14116f35661a6afb71f466754fd445d80f0311`.
No new device or hardware-custody qualification is claimed. TLS stays quarantined.
Review: [C_SAFETY_REVIEW.md](C_SAFETY_REVIEW.md). Exact baseline/promoted manifests,
public probes, negative evidence, corpus and logs are preserved in
`.cache/android-wallet/resume-20260915/integer-safety/`.

2026-09-15: integer-safety checkpoint
`ec15d585743418d89f4716670c570a2646bd2c7d` is verified on the private
`wallet-backup` development branch. Local/tracking/direct-Git/GitHub-API SHAs
agree; the tree was clean, upstream correct and ahead/behind 0/0. All 119 wallet
commits remain preserved, including the original 108-commit checkpoint. Origin
remains unchanged; only the same-named development branch was pushed.

2026-09-15: setup expiry now checks elapsed time at use. Previously, the only
ten-minute setup limit was a delayed Handler callback. Recovery entry/retry and
queued worker operations could proceed without checking their age. Two API30
regressions against unchanged Activity routing reproduced expired entry/retry
displaying a recovery keyboard. The fixture injects public clock values on the
storage-free debug host; it does not wait ten real minutes or mutate device time.

C now computes a bounded positive remaining delay, refuses backward/expired
timestamps and preserves the output on failure. JNI rejects negative signed
timestamps. One immutable public clock origin is created after authentication
approval, before secret work, and shared by the UI and worker. Display, retries,
submission and actual worker processing recheck it. Sealing checks again before
encryption and before starting persistence. An already-admitted C persistence
operation completes its existing durability protocol; no unsafe partial-write
cancellation is introduced. Timers schedule cleanup when Android can run them;
this does not claim instantaneous secret erasure during suspension or queue stalls.

Eight new instrumentation checks cover expired recovery entry/retry/submission,
queued confirmation/restoration/admission and live malformed-input controls one
millisecond before expiry. They use public marker arrays, an uninitialized cipher
and unique empty cache parents, with no wallet creation, key generation, prompt
or authentication bypass. The worker fixtures hold the actual queue, advance the
public clock and then release it. Expired owners are cleared before persistence
or phrase decoding; live malformed input retains setup with its original deadline.
Together with the four backup-screen failure tests, all 12 checks pass on x86-64
API30/API35/API36 emulators in 74.342/164.773/135.394 seconds. Existing profiles
remain intact. These debug fixtures do not qualify successful hardware custody,
minified setup UI behavior or elapsed expiry during a real provider operation.

All 95 native fuzz/oracle groups pass in 113.91 seconds. The C gate first refused
a fuzzer function with complexity 16; splitting its setup translation checks
preserved every assertion and restored the unchanged 10/15 caps. The final C
safety gate passes all 91 groups in 57.52 seconds, plus strict Clang/GCC compilation
and analysis. The expanded time fuzzer completes 69,360,514 cases in 121 seconds
without a finding (max_len24, timeout5, RSS cap512 MiB, observed303 MiB). Five
isolated C mutations exercise late/early expiry, missing backward-clock refusal,
incorrect remaining delay and premature output publication; all refuse for their
intended invariant. Native/JVM tests also cover exact limits and negative-equal
JNI timestamps that unsigned conversion could otherwise reopen.

Android/JVM builds/tests, debug/release lint, fixture isolation, architecture and
16 KiB native alignment pass. The unsigned release APK is 630055 bytes, 16704
bytes larger than the prior checkpoint; SHA256 is
`2536e39993caffff70f27dce19178d559f684b3a82b6e0d78fca972339ee72d8`.
This is an artifact-size observation, not a RAM/startup performance claim.
Review: [C_SAFETY_REVIEW.md](C_SAFETY_REVIEW.md). Baseline source/APKs/failures,
public fixtures, complexity refusal, mutations and exact final acceptance remain
in `.cache/android-wallet/resume-20260915/setup-expiry/`. TLS remains quarantined;
positive hardware custody remains unqualified.

2026-09-15: elapsed-setup checkpoint
`cc65321b82eedc879c7076c195f9a507e76e7cc6` is verified on the private
`wallet-backup` development branch. Local/tracking/direct-Git/GitHub-API SHAs
agree; the tree was clean, upstream correct and ahead/behind 0/0. All 120 wallet
commits remain preserved, including the original 108-commit checkpoint. Origin
remains unchanged; only the same-named development branch was pushed.

2026-09-15: the scalar JNI clock projection now shares its timestamp conversion
and refusal handling. The C authentication predicate and separate setup policy
retain their 90000/600000-ms limits. A new C authentication-remaining function
reuses that predicate and preserves output on failure. The managed WrappingPolicy
API is unchanged: zero-age authentication queries return its full duration, while
actual delivery still checks elapsed time. No hardware-authentication policy,
secret operation, storage protocol or native alignment setting changes.

This follows an exact APK measurement: setup expiry increased the ARM library
across the ZIP's next 16 KiB alignment boundary. Three JNI clock exports became
one, and both native libraries are 256 bytes smaller. The ARM data offset remains
98304; the following x86 data offset falls from 344064 to 327680. This recovers
one alignment page without weakening alignment. The 17 non-code APK entries
remain byte-identical; classes.dex decreases by 76 bytes.

| Release artifact | Before | After |
| --- | ---: | ---: |
| Unsigned APK | 630055 bytes | 613415 bytes |
| ARM64 native library | 229488 bytes | 229232 bytes |
| x86-64 native library | 259264 bytes | 259008 bytes |

The APK reduction is 16640 bytes. This is a file-size result; no RAM, CPU or
startup-time improvement is claimed. Release SHA256 is
`3578329a69f52cbc868870505430532bbf2cd812cecbbd29da8975d65de24e72`.

The new registered scalar JNI fixture checks both deadlines, 32/64-bit crossings,
negative-equal timestamps and all 254 invalid boolean byte values. The C
remaining-delay fixture now covers both named policies without removing its prior
assertions. Four isolated JNI mutations (boolean refusal, signed refusal, selector
swap and truncation) and an authentication failure-output mutation all refuse for
their intended invariant. The expanded time fuzzer completes 62,374,271 cases in
121 seconds without a finding (max_len24, timeout5, RSS cap512 MiB, observed276 MiB).

All 96 fuzz/oracle groups pass in 113.56 seconds and all 92 default native groups
pass in 57.67 seconds. Strict Clang/GCC analysis, unchanged complexity caps10/15,
architecture, Android/JVM builds/tests, debug/release lint, fixture isolation and
16 KiB alignment pass. The unchanged two authentication and eight setup tests
pass through actual device JNI on API30/API35/API36 in 35.909/110.266/65.949 seconds.
The scalar C fixture also passes on all three x86-64 APIs linked dynamically to
the exact extracted release library; host/device hashes agree. Its ARM64 build
is compile-only. No VM operations, keys, wallet storage or provider authentication
are used by that release-library fixture. Positive hardware custody and minified
setup UI remain unqualified; TLS stays quarantined.

Review: [C_SAFETY_REVIEW.md](C_SAFETY_REVIEW.md). Baseline/candidate ZIP layouts,
library/export identities, negative sources, public corpus, APK hashes and all
acceptance logs are preserved in
`.cache/android-wallet/resume-20260915/setup-apk-size/`.

2026-09-15: shared-clock checkpoint
`2f217d537cfbd344d9ad3f341eef1cc6ead9ed11` is verified on the private
`wallet-backup` development branch. Local/tracking/direct-Git/GitHub-API SHAs
agree, upstream is correct and the tree is clean with ahead/behind 0/0. All 121
wallet commits remain preserved; origin is unchanged.

2026-09-15: the unsigned Android release now reproduces across checkout paths
on the same host. Before this change, an exact tracked-source archive rebuilt
in an isolated cache directory produced a different APK. Only the two native
libraries differed, each exactly in the 20-byte ELF build-ID payload at file
bytes 793–812. Absolute debug paths affected the content-derived IDs even though
those debug sections were stripped from the packaged libraries. No other APK
entry differed. The baseline source, APKs and byte comparisons are preserved.

CMake now applies one Android-only `-ffile-prefix-map=<checkout>=.` option before
all targets, including provider subdirectories. The actual release compilation
manifests contain the mapping for all 98 translation units in each ABI and each
build directory. The link still uses `--build-id=sha1`; IDs are neither removed
nor assigned a constant. Host sanitizer/fuzz settings and Android hardening,
visibility and 16 KiB alignment remain unchanged.

The ordinary build and a fresh relocated build now produce byte-identical
613415-byte unsigned APKs, SHA256
`6f9b9641dfeecd36cca4b110dc3aba506bcef4d1aef8fd7bea2d0390bb567a1f`.
Relative to the prior release, both libraries change only within the build-ID
payload; every other library byte and every other APK entry remains identical.
The complete Android/JVM build/test/lint, fixture isolation and native-alignment
run passes; the relocated offline release executes all 57 tasks successfully.
The earlier path-dependent build is the preserved negative control.

All 92 default native sanitizer groups pass in 57.34 seconds, together with
strict Clang/GCC analysis, unchanged complexity caps 10/15 and architecture.
The Android native code bytes are unchanged outside build IDs, so this build
qualification does not claim new runtime or device-custody acceptance. The
measured scope is same-host, same-toolchain path independence; signed APKs,
different hosts/toolchains and positive hardware custody remain unqualified.
TLS remains quarantined. Review: [C_SAFETY_REVIEW.md](C_SAFETY_REVIEW.md).
Source archives, exact CMake patch, negative/positive APKs, compilation manifests,
byte comparisons and validation logs remain in
`.cache/android-wallet/resume-20260915/apk-reproduction/`.

2026-09-15: checkout-path reproduction checkpoint
`7f6f837319bbd7f9c2be7a78357ba30f18cf38eb` is verified on the private
`wallet-backup` development branch. Local/tracking/direct-Git/GitHub-API SHAs
agree, upstream is correct and the tree is clean with ahead/behind 0/0. All 122
wallet commits remain preserved; origin is unchanged.

2026-09-15: setup sealing now has device failure-path acceptance past the GCM
boundary. Five new instrumentation tests exercise actual worker, canonical C
backup confirmation, sealing and isolated persistence with a nonzero public
entropy marker, fixed public AES key and fixed IV. There is no entropy generation,
Keystore alias or authentication prompt. A CipherSpi observer delegates real
software GCM through a provider passed only to that Cipher instance; it is never
registered globally or submitted to production hardware-custody acceptance.

The tests verify expiry after successful GCM completion refuses persistence;
AAD and finalization exceptions clear session entropy and submitted recovery
words; and a live operation commits its public fixture and clears both owners.
A separate observer control checks actual GCM parameters and completion. The
finalization observer borrows the exact input array, allowing the test to verify
its erasure after the worker drains. The nonzero marker makes omitted wiping
observable. Failure cases require the storage directory to remain absent.

All five tests pass on API30/API35/API36 in 1.052/4.812/2.166 seconds. Two isolated
mutations in a separate cache-source tree and application ID qualify the checks:
removing the post-encryption expiry guard fails exactly the expiry test while
four controls pass; removing Setup entropy erasure fails all four wallet-path
cleanup tests while the observer control passes. The expired mutant's unexpected
public fixture files are retained as evidence. Its separate emulator package is
disabled after validation; ordinary wallet source and package remain unchanged.

Initial fixture failures are also preserved. An Android supplied-SPI Cipher
subclass skipped the observer's initialization, so its delegate lacked GCM
parameters. A standalone control exposed that fixture assumption. The final
instance-local provider uses normal initialization; production's missing-parameter
refusal was never weakened. The relevant Android11 implementation is
[Cipher.SpiAndProviderUpdater](https://android.googlesource.com/platform/libcore/+/refs/tags/android-11.0.0_r1/ojluni/src/main/java/javax/crypto/Cipher.java).

Android/JVM tests, debug/release builds and lint, fixture isolation and 16 KiB
alignment pass. The unsigned release remains byte-identical to the prior
checkpoint: 613415 bytes, SHA256
`6f9b9641dfeecd36cca4b110dc3aba506bcef4d1aef8fd7bea2d0390bb567a1f`.
No production C/Kotlin behavior, ABI, complexity threshold or sanitizer setting
changes. This extends software-provider routing/cleanup evidence, not hardware
authentication, fatal-VM-error, process-death or signed/minified custody acceptance.
TLS remains quarantined; positive hardware custody remains unqualified.

Review: each fixture has one session/worker, bounded waits, an independent
callback queue and a unique cache parent. Cleanup joins that worker before
clearing fixture arrays. Success deletes only its known public fixture files;
unexpected files are preserved, with no recursive wallet deletion. The observer
retains only the borrowed public input array and no secret copy. No fixture or
fault hook enters the release. Exact sources, APKs, initialization failures,
mutations and acceptance logs remain under
`.cache/android-wallet/resume-20260915/setup-seal/`.

2026-09-15: setup-sealing checkpoint
`2eb9c4c1f99b42687aa26c83f7d138ac75543be5` is verified on the private
`wallet-backup` development branch. Local/tracking/direct-Git/GitHub-API SHAs
agree, upstream is correct and the tree is clean with ahead/behind 0/0. All 123
wallet commits remain preserved; origin is unchanged.

2026-09-15: the JNI secret-erasure regression/fuzz observer now stores integer
span identities instead of retaining stack pointers. An omitted-wipe mutation
can end a child buffer's lifetime before a later comparison; the observer must
remain valid even when the code under test fails cleanup. Pointer-to-integer
conversion occurs only through live arguments. Bytes are inspected only through
the current live zeroizer argument. All prior cleanup assertions remain intact.

The audit also reproduced a deterministic regression gap: changing only the
recovery-input wipe from 430 bytes to 429 passed the prior fixed JNI suite at
Clang20 -O2 with sanitizers. Four new cases fill the complete 215-character input
for restoration and confirmation, using an invalid ASCII word and a non-ASCII
final character with both bytes nonzero. JNI copies the full input before either
refusal, so all 430 bytes must be wiped. The shortened-wipe mutation now fails
its minimum-length assertion. Separately omitting recovery-input, phrase-output
or entropy wiping also fails the intended cleanup assertion; the control passes.
The production wipe was already correct; this change strengthens its evidence.

All 92 default native sanitizer groups pass in 57.17 seconds. Strict Clang/GCC
analysis and unchanged complexity caps10/15 pass, including separate optimized
analysis of the regression and fuzz fixture forms. Counts are512 production
functions/91 files and1298 fixture functions/157 files. The largest measured
GCC-O2 fixture frame is1408 bytes; this is not a whole-call-stack bound.
The existing JNI fuzzer runs60,996 cases in121 seconds without a finding, with
max_len217, per-input timeout5, RSS cap512 MiB and observed55 MiB. ASan stack-use-
after-return, leak detection and strict string checks remain enabled. Its public
corpus includes explicit full-length restoration/confirmation inputs.

The updated native fixture also passes on x86-64 Android API30/API35/API36,
compiled at NDK-O2 against the current release C/provider archives; transferred
executable hashes match the host. ARM64 compilation passes but is not executed.
This fixture uses a fake JNI environment and fixed synthetic RNG bytes, with no
VM, wallet file, fresh entropy source or Keystore operation. It does not newly qualify
real-VM or hardware custody behavior.

Android/JVM tests/builds, debug/release lint, fixture isolation, architecture and
16 KiB alignment pass. Production source and the unsigned release APK remain
unchanged; the APK still hashes to
`6f9b9641dfeecd36cca4b110dc3aba506bcef4d1aef8fd7bea2d0390bb567a1f`.
Review: [C_SAFETY_REVIEW.md](C_SAFETY_REVIEW.md). The escaped mutation, original
fixture, strengthened controls, optimized negative binaries, corpus and all
acceptance logs remain in
`.cache/android-wallet/resume-20260915/jni-span-identity/`.
TLS remains quarantined; positive hardware custody remains unqualified.

2026-09-15: JNI erasure-fixture checkpoint
`c68fb89aab42dc3c0085e6ffdd120c43eb5bf029` is verified on the private
`wallet-backup` development branch. Local/tracking/direct-Git/GitHub-API SHAs
agree, upstream is correct and the tree is clean with ahead/behind 0/0. All 124
wallet commits remain preserved; origin is unchanged.

2026-09-15: recovery-phrase encoding and entropy restoration now validate the
caller-owned JNI destination before copying or processing recovery input. Null
or incorrectly sized output is refused without creating native secret copies.
Previously the same refusal happened after copying/processing input, followed by
correct wiping; this is reduced secret handling on failure, not a discovered
production leak. Output sizes, successful values and Java APIs are unchanged.

The existing destination check moves to entry, with a null-input short circuit.
Its private writer no longer repeats that check: the same Java array has an
immutable length, and successful input JNI reads have already checked pending
exceptions. Valid entry points retain the same four array operations and six exception checks. The writers
still check derived result lengths and observe exceptions after transfer. Native
scratch wiping and managed ownership of partially transferred output remain
unchanged, with no new allocation, JNI reference, capacity or policy.

The native regression requires zero touched secret spans for every existing
invalid/null destination case. The preserved baseline fails that requirement.
Two separate optimized mutants move validation back to its old position for
encoding or restoration individually; both fail the intended zero-copy assertion
while the unchanged control passes. No output bounds are removed in these mutants.
All 96 fuzz/oracle groups pass in113.28 seconds and all92 default native sanitizer
groups pass in57.56 seconds, together with strict analysis and complexity gates.

Android/JVM tests/builds, debug/release lint, isolation, architecture and16 KiB
alignment pass. Two unchanged public secret-destination/exception instrumented
tests pass on API30/API35/API36 in0.851/3.006/0.971 seconds. The updated native
fake-JNI fixture also passes all three x86-64 APIs against current release
archives, with verified transferred hashes; ARM64 is compile-only. The host
real-JVM partial-transfer/OOM fixtures remain part of the passing JVM suite.
These tests use public vectors and no fresh entropy source or hardware key.

The security change increases the ARM library by96 bytes to229328 and x86 by64
bytes to259072. The ARM payload crosses the next16 KiB APK alignment boundary;
the unsigned APK grows613415→629863 bytes (+16448). All other APK entry contents
are byte-identical. Alignment remains intact. Release SHA256 is
`f81cbe93df5d7d4591d1dd7ee417c4c0be3edb8d2c57898fbe49448175c7ff8f`.
This is an artifact-size cost, not a RAM/startup/latency claim. Further size work
must preserve early refusal, ownership, cryptography and alignment.

The existing JNI fuzzer completes78,049 cases in121 seconds without a finding,
with max_len217, per-input timeout5 and RSS cap512 MiB (observed57 MiB).
ASan stack-use-after-return, leak detection and strict string checks are enabled.
Review: [C_SAFETY_REVIEW.md](C_SAFETY_REVIEW.md). Baseline failure/source/APK,
independently delayed-validation mutants, public corpus, exact artifact sizes
and all acceptance logs remain in
`.cache/android-wallet/resume-20260915/jni-destination-preflight/`.
TLS remains quarantined; positive hardware custody remains unqualified.

2026-09-15: early-destination-validation checkpoint
`5b13b32e03e8b3be6cce15fd0e0088dad2d4c772` is verified on the private
`wallet-backup` development branch. Local/tracking/direct-Git/GitHub-API SHAs
agree, upstream is correct and the tree is clean with ahead/behind 0/0. All 125
wallet commits remain preserved; origin is unchanged.

2026-09-15: Android JNI adapters now compile with -Os, retaining -O2 for
`jni_keys.c`. This measured profile preserves the previous milestone's early
secret-destination refusal and reduces the unsigned APK from 629863 to 611783
bytes (-18080). ARM64/x86-64 libraries are 228176/257376 bytes (-1152/-1696).
All 18 non-native APK entry contents and all seven C core/provider archives per
ABI remain byte-identical. ELF hardening and 16 KiB ELF/ZIP alignment remain
intact. No source algorithm, cryptographic provider, host-build flag or ABI
changes. The actual Android manifests contain 17 JNI units per ABI; only the
key adapter retains effective -O2 within that target.

The first isolated candidate applied -Os to every JNI source. Its APK was
611367 bytes, but a public phrase-conversion CPU probe measured encode/decode
median increases of 19.24%/4.11%. That candidate is preserved and not promoted.
Keeping the key adapter at -O2 costs 416 APK bytes and restores its original
object bytes for both ABIs. The repeated probe observes no regression: baseline
encode/restore medians 569114410/1561742523 ns per 100000 calls, accepted profile
557956084/1502616776 ns. Ranges overlap; this is not a general speedup claim.

Both probes use the same NDK executable, a fixed fake-JNI environment and the
exact packaged release libraries on an x86-64 API30 emulator. Public zero-entropy
mnemonic conversion only: no RNG, seed derivation, wallet file, authentication
or actual VM. Each variant has ten CPU-time samples per operation, collected
in ABBA process order after 2000 warmups, with byte/length/guard checks. No
physical ARM, startup, RAM, battery or end-to-end performance claim is made.

All 92 default native sanitizer groups pass in 57.22 seconds; strict Clang/GCC
analysis and unchanged complexity caps10/15 pass. A separate cache-only host
profile applies the same JNI optimization choices without removing ASan, UBSan
or authored integer checks. All 12 JNI groups pass in 2.60 seconds with stack-
use-after-return, leak and strict-string checks. Its manifest confirms all 51
JNI source compilations retain those sanitizer flags and the intended final
optimization flag; normal host profiles remain unchanged.

Under that optimized sanitizer profile, the existing JNI key and storage
fuzzers complete 73499/29196 cases respectively in 121 seconds each without
a finding. Maximum inputs are 217/8 bytes, per-input timeout5 and RSS cap512
MiB; observed peaks are 58/47 MiB. Public corpora and failure artifacts remain
in the owned cache directory. Storage fuzzing uses only its isolated fixture.

The 36 selected real-JNI instrumented tests pass on API30/API35/API36 in
16.464/66.175/37.426 seconds, covering public secret conversion, records,
isolated storage/recovery, QR decoding/isolation, payment input, offline
sync/history, unsigned review, authentication timing and setup-seal failures.
The native fake-JNI secret-erasure fixture also passes all three x86-64 APIs
with the selected per-source optimization and current C/provider archives;
transferred executable hashes agree. ARM64 is compile-only. This instrumented
fixture uses synthetic RNG and a wipe observer, rather than the exact packaged
JNI object; real-VM tests and the CPU probe cover the packaged libraries.

Android/JVM tests, debug/release builds and lint, fixture isolation, architecture
and alignment pass. The ordinary and relocated exact-source builds produce
the same unsigned APK SHA256:
`67da00892ae1dcedfc72a1f66997b7d1630d7075ac2d3cfcfbcee1a60b974120`.
This remains same-host/toolchain path-independence evidence. Review:
[C_SAFETY_REVIEW.md](C_SAFETY_REVIEW.md). Baseline and both candidates, raw CPU
samples, source archive, compile manifests and acceptance logs remain under
`.cache/android-wallet/resume-20260915/jni-size/`.
TLS remains quarantined; positive hardware custody remains unqualified.

2026-09-15: measured-JNI-size checkpoint
`322b6d62c26557553309d04a9caa785808b4d2a7` is verified on the private
`wallet-backup` development branch: four-way SHA agreement, correct upstream,
clean tree and ahead/behind 0/0. All 126 wallet commits remain preserved.

2026-09-15: recovered the storage JNI guard and full-width entropy regression
from preserved commit `4057097c333296e9a1cf97a2c8c7caac9909a53a`. Its source
parent matches these two files on the current wallet branch. Applied only the
reviewed source/test patch here, without switching worktrees, merging, rewriting
history or importing old artifacts/acceptance claims. Original work is preserved.

`readWalletStorage` now refuses a missing JNI environment before dereferencing
it, and refuses a pending exception before array work. The current optimized
ASan/UBSan/integer build reproduces the baseline null load. Real JNI calls
supply their environment; this is a defensive native-call fix, not evidence of
an Android exploit. A non-NULL environment still requires the VM's valid table.

The storage erasure fixture now fills all 32 entropy bytes with public nonzero
values, verifies the actual native copy is zero while live, preserves borrowed
input, and checks the resulting paired storage. Independently wiping only 16
or 31 bytes still passes the old fixture but fails the new byte assertion.
The unchanged control passes. Each mutation uses an owned directory retained
as evidence; no real wallet, seed or GCM authentication claim is involved.

The first NDK fixture compile exposed its host-only JNI table tag. Reused the
existing key fixture's Android/host typedef pattern; no VM layout is invented.
The final native fixture passes x86-64 API30/API35/API36 with matching transferred
hashes, current release archives and -Os JNI sources. ARM64 is compile-only.
Ten existing real-VM storage/GCM/recovery tests pass per API in
6.328/25.136/11.893 seconds. Their provider keys and records are isolated public
fixtures; no wrapping-key alias or production wallet is used.

All 92 default sanitizer groups pass in 57.59 seconds; the optimized JNI profile
passes 12/12 in 2.62 seconds. Final host fixture compilation/static analysis and
its focused sanitizer group pass after the portable table typedef. Strict
analysis, complexity caps10/15, Android/JVM tests, debug/release builds and lint,
fixture isolation, architecture and16 KiB alignment pass. An invalid scratch
lint-mode spelling was refused; the final check uses the gate's existing FAIL
mode and verifies zero test functions above15, without changing any threshold.

The optimized storage fuzzer completes31899 cases in121 seconds without a
finding: max_len8, timeout5, RSS cap512 MiB (observed48 MiB), with stack-use-after-
return, leak and strict-string checks. It covers fresh-storage JNI faults;
the read-environment guard has explicit regression coverage.

The unsigned APK grows32 bytes to611815; ARM64/x86-64 libraries grow16/32 bytes
to228192/257408. All18 non-native APK entries are unchanged. Release SHA256:
`8e4dcf879df3fee113acffb0c2ab3ceb8c01434612eb58fbd366554d438f6421`.
Review: [C_SAFETY_REVIEW.md](C_SAFETY_REVIEW.md). Recovered patch identity,
baseline failure, mutation evidence, NDK header refusal, public corpus and
current acceptance logs remain under
`.cache/android-wallet/resume-20260915/storage-refusal/`.
TLS remains quarantined; positive hardware custody remains unqualified.

2026-09-15: storage-refusal checkpoint
`afaea0a73276b1c8322cebfea873397bca6578fd` is verified on the private
`wallet-backup` development branch: local/tracking/direct-Git/GitHub-API SHAs
agree, upstream is correct and the tree is clean with ahead/behind0/0. All127
wallet commits remain preserved; origin is unchanged.

2026-09-15: camera-to-UI handoff now retires and wipes a still-queued pixel
packet when callback creation or Handler posting throws. Previously only a
false post result and normal close cleared that field, so an exceptional post
could retain the image bytes and leave frame admission marked busy. Cleanup is
now in finally, conditioned on successful posting. The original exception
propagates; a late queued callback finds no packet to claim. A callback that
already claimed ownership retains its existing finally wipe. No extra buffer,
queue, worker, retry, camera permission or cryptographic behavior is introduced.

Four new instrumented tests use public21x21 pixel packets and a private Handler
on the real main Looper. They inject RuntimeException/OutOfMemoryError before
enqueueing and after a successful real enqueue, then verify complete erasure,
retirement, released frame admission and unchanged exception identity. Late
callbacks must remain inert without being removed before that assertion.
False posting, closed capture and successful single delivery are controls.
The baseline fails both exceptional-handoff tests while the controls pass;
the identical fixture passes the fix. No actual memory is exhausted or camera
started, and no wallet, seed, permission or key is accessed.

Together with existing camera-start and preview-failure coverage, all12 selected
instrumented tests pass API30/API35/API36 in1.547/13.364/5.415 seconds.
Android/JVM tests, debug/release builds and lint, fixture isolation, architecture
and16 KiB alignment pass. Native source and both packaged native libraries
remain byte-identical to the preceding validated milestone, retaining its
sanitizer/fuzz/complexity evidence. This change makes no new physical camera,
driver cancellation, global-OOM or hardware-custody claim.

The unsigned APK remains611815 bytes; only classes.dex changes. SHA256:
`a60d1da861c791fe50c49eafe77be66fb03bcdaa710ae5148d5117d2afae0cca`.
Reviewed atomic ownership transfer, close/post interleavings, fatal propagation
and allocation-free failure cleanup. The worker still owns camera resources;
this fix concerns queued pixel ownership. Baseline source/APKs, the failing
device result, unchanged fixture identity and final acceptance logs remain in
`.cache/android-wallet/resume-20260915/camera-dispatch/`.
TLS remains quarantined; positive hardware custody remains unqualified.

2026-09-15: completed the recovered caller-owned camera JNI output candidate.
The managed caller now allocates the exact destination before native packing,
so a partial VM write followed by an exception still has an owner that wipes
the complete pixel array. C independently revalidates geometry and destination
capacity before its one exact allocation, and always wipes native scratch.
The real-JVM fixture observes prefixes 0/1/6/223/446 before an injected exception;
the preserved baseline fails its pixel-erasure assertion. Bounds, refusal,
unchanged borrowed inputs and successful transfer without copying are covered.

All 92 native sanitizer groups pass again in 57.32 seconds, with strict Clang/GCC
analysis and unchanged production/test complexity caps 10/15. Android/JVM,
debug/release builds and lint, fixture isolation and native alignment pass.
Native fake-JNI fixtures pass x86-64 API30/35/36; both ABIs compile. The existing
C camera fuzzer completed 2,811 cases in 121 seconds without a finding, with
ASan/UBSan/integer checks, stack-use-after-return, leaks and strict strings;
maximum input 8,388,616 bytes, timeout 5 seconds, RSS cap 512 MiB, observed 264 MiB.
That campaign covers C packing and decoding, not a new JNI fuzz target.

All 10 selected camera/output/decoder/handoff instrumented tests pass per API:
API30 7.619 s, API35 43.804 s, API36 32.634 s. An earlier API35 aggregate exceeded
its 120-second shell bound. Its two output tests separately passed in 71.93 s.
Bytecode inspection showed boxed per-byte JUnit calls in the new exhaustive
fixture. Replaced those calls with inline complete-span predicates: every byte,
layout, offset, read-only view and boundary assertion remains checked, with no
extra buffer or increased timeout. The final aggregate passes the same bound;
the earlier timeout and logs remain preserved. This is fixture-cost evidence,
not an application camera latency claim.

Unsigned release size is 612,647 bytes (+832); SHA256
`619fe1745d35c145bf29bde7b96ad2fe8b1510cc111998b38cdb91906f505c8d`.
Only classes.dex and the two JNI libraries change; core/provider archives remain
byte-identical. Exact sampled allocations and 16 KiB alignment are preserved.
The new size query adds a JNI roundtrip; no speed or battery gain is claimed.
Review: [C_SAFETY_REVIEW.md](C_SAFETY_REVIEW.md). Evidence is retained under
`.cache/android-wallet/resume-20260915/camera-jni-output/`. TLS stays quarantined;
hardware custody and physical ARM64 runtime acceptance remain unqualified.
Next: authenticated-decryption destination ownership under provider exceptions.

2026-09-15: authenticated unlock now allocates its exact 16..32-byte plaintext
destination before calling the GCM provider. The write and exact returned-length
check are inside the owner's finally block, which clears the full destination
on success, refusal and fatal exceptions. Previously the allocating doFinal
overload could write plaintext and throw before returning its array to that
owner. C record/recovered-address validation, per-use authentication and storage
promotion order are unchanged. No native or cryptographic algorithm changes.

The real Android worker regression reproduces both baseline failures with a
test-only provider that delegates real software GCM, observes public nonzero
plaintext, then throws AEADBadTagException or OutOfMemoryError. The original
normal path passes all five supported entropy lengths. Both failures pass with
the fix. Additional controls cover corrupted ciphertext, six incorrect returned
lengths, unchanged borrowed ciphertext and closing while the provider still
owns its write. Closing retains the active buffer until the worker clears it;
the original fatal exception propagates and retired callbacks remain inert.
No actual memory exhaustion, hardware alias, authentication prompt, real seed
or funded wallet is used. All filesystem fixtures have privately created paths
and exact known-file cleanup; refusal cases create no storage.

All 21 selected unlock/setup/GCM/storage/recovery instrumented tests pass on
x86-64 API30/API35/API36 in 7.382/23.016/15.856 seconds. Android/JVM tests,
both-ABI debug/release builds, lint, fixture isolation and 16 KiB alignment pass.
Only classes.dex changes; both native libraries remain byte-identical to the
preceding 92-group sanitizer/analysis/fuzz checkpoint. Unsigned release remains
612,647 bytes, SHA256
`bd525af260b43b4516a199e5e0bde1add1c570c79611e6288d2fba374f31b1b5`.

Reviewed bounded subtraction/allocation, exact output lengths, one managed
owner, worker/close ordering, error propagation and cleanup before callback
delivery. This erases the caller's buffer; it cannot erase every internal
provider/VM copy. Positive hardware-backed custody remains unqualified, and TLS
remains quarantined. Source, baseline failures, unchanged test APK inputs,
device logs and byte comparisons are retained in
`.cache/android-wallet/resume-20260915/unlock-output/`.
Next: shorten submitted recovery-phrase lifetime after native confirmation or
decoding, before subsequent encryption and persistence work.

2026-09-15: submitted recovery words are now erased immediately after native
confirmation or decoding. Subsequent derivation, GCM and disk IO retain only
the entropy they require. The existing submission cleanup still erases words
for queued cancellation, refusal and failures before the native call; repeated
erasure is allocation-free and idempotent. Invalid restoration clears its words
before posting the invalid-input callback. No decoding, confirmation, seed,
address, authentication or persistence behavior changes.

Both creation and restoration baseline regressions observed nonzero consumed
words at the GCM boundary, then injected a provider refusal and verified final
cleanup without storage. Both now observe erased words while the active entropy
is intact. A new successful restoration control commits the same public address
and observes the decoded entropy's cleanup. Existing creation, seal failure,
expiry, unlock and worker cancellation controls remain intact. The 21 selected
instrumented tests pass x86-64 API30/35/36 in 4.895/16.424/9.899 seconds.

Android/JVM tests, both-ABI builds, debug/release lint, fixture isolation and
alignment pass. Native source and both packaged native libraries are unchanged,
retaining the preceding sanitizer/fuzz/complexity evidence. Unsigned release
remains 612,647 bytes, SHA256
`040773ce811d30d014f3c29c595fb126e15b13c7bf1d2352b8dee26bd6c69805`.
Reviewed last-use ownership, no copy/queue growth, failure and callback order,
and unchanged entropy lifetime during provider writes. This bounds our managed
phrase lifetime; it makes no claim about all VM/UI copies or hardware custody.
Evidence: `.cache/android-wallet/resume-20260915/phrase-retirement/`.
Next: dedicated fuzz coverage for the camera JNI size/fill boundary, preserving
the existing independent C pixel/reference and real-VM erasure checks.

2026-09-15: added the dedicated `fuzz_jni_camera` host target, reusing the real
JNI adapters and existing allocation/exception observer. Thirteen public
control bytes vary geometry, direct offsets/capacities, output lengths, missing
arguments, allocation refusal, pending VM exceptions and partial-write prefixes.
The fake VM never advertises more backing memory than it owns. Exact C/JNI
packet comparison, immutable inputs, output sentinels and full scratch erasure
are checked; the independent C pixel oracle and real-JVM ownership tests remain.
The ordinary JNI test also runs 273 deterministic control mutations.

All 92 native sanitizer groups pass in 57.82 s. Strict Clang/GCC source analysis,
both fixture profiles and unchanged complexity caps 10/15 pass. Actual compile
manifests verify coverage and ASan/UBSan/authored-integer instrumentation.
The bounded fuzzer completes 15,230 executions in 121 s without a finding,
max_len 13, timeout 5 s, RSS cap 512 MiB, observed 95 MiB. Native fixture runs
pass x86-64 API30/35/36 with exact transfer hashes; ARM64 compiles. Android/JVM,
both-ABI builds, lint, isolation and alignment pass. The entire unsigned release
APK remains byte-identical to the prior slice. Safety review and evidence are
under [C_SAFETY_REVIEW.md](C_SAFETY_REVIEW.md) and
`.cache/android-wallet/resume-20260915/jni-camera-fuzz/`.

2026-09-15: decoded QR replies now retire their text and request if posting to
the main queue throws, including a post that enqueued before throwing. The
existing failure path closes the client and attempts one UI notification; a
secondary notification exception cannot replace the original handoff error.
An already-queued reply observes a retired request and cannot deliver it.
Caller identity, request IDs, native parsing, elapsed deadlines and ordinary
successful/false-post behavior are unchanged.

The baseline fails three new exceptional-handoff tests while false-post and
successful delivery controls pass. The final five-test fixture covers runtime
and injected OOM failures before/after enqueue, a failing error notification,
complete byte erasure, request retirement and one successful delivery. It uses
a private Handler on the real main Looper with public decoded text; its
synthetic pending request/UID does not claim Binder isolation. Separate real
isolated-service, deadline and QR checks run alongside it: all 12 selected
tests pass x86-64 API30/35/36 in 13.541/63.530/43.345 seconds.

Android/JVM, both-ABI builds, debug/release lint, fixture isolation and alignment
pass. Native libraries remain byte-identical, retaining the preceding native
safety evidence. Unsigned release remains 612,647 bytes, SHA256
`02439fbb59352322e6f4f481f5d55127d8953d6b50c2290708cf7a2d9000d797`.
Reviewed atomic claim/close races, clearing before notification, late callbacks
and preservation of the primary exception. A callback that already completed
delivery cannot be undone; the queued-callback tests hold the main thread until
retirement to observe the specific stated invariant. No camera, secret input,
wallet or actual OOM is involved. Evidence is retained in
`.cache/android-wallet/resume-20260915/scan-reply-handoff/`.

2026-09-15: exceptional decoder-readiness handoffs now close and unbind the
client after its connection timeout has been retired. A readiness callback
queued before the exception sees the closed client and remains inert. The
original exception is preserved even if failure notification also fails.
Identity validation, elapsed deadlines and successful readiness are unchanged.

A new actual isolated-service fixture refuses readiness posts before/after
enqueue, holds UI delivery until Binder cleanup completes, and checks the
isolated UID, one failure callback, no readiness delivery and no retained bind.
Its baseline fails both exceptional paths; ordinary readiness passes. All 12
selected readiness/reply/isolation/deadline tests pass x86-64 API30/35/36 in
15.263/61.511/43.256 seconds. No deadline or test timeout was relaxed.
Android/JVM, both-ABI builds, lint, fixture isolation and alignment pass. Native
libraries remain byte-identical. Unsigned release is 612,647 bytes, SHA256
`0c1fab6ac6673694de8a64a42badae42f614944ab1e5576322548f563f4b015d`.
Reviewed Binder/main ordering, atomic closure, unbinding and original-error
preservation. The fixture uses no camera, decoded input, wallet or key. Evidence:
`.cache/android-wallet/resume-20260915/scan-ready-handoff/`.
Next: erase unlocked entropy at the address-derivation boundary, before storage
promotion and UI dispatch. The existing bounded EC allocation is retained:
its opaque provider storage needs suitable alignment and C effective-type
semantics; replacing it mechanically with a declared byte array is unjustified.

2026-09-15: unlock now erases its bounded plaintext immediately after authenticated
address derivation, before native storage promotion and UI scheduling. The
public address and exact authenticated ciphertext suffice for those operations;
no secret needs to survive a slow durability operation. Decryption, recovered
address verification, no-overwrite promotion and callback order are unchanged.

The enhanced real-worker/public-GCM fixture snapshots full output erasure at the
UI executor boundary. The old implementation fails that assertion; the new one
passes for all five supported entropy lengths, with unchanged exact addresses
and successful storage verification. All 21 selected unlock/setup/expiry/worker
tests pass x86-64 API30/35/36 in 5.750/17.975/10.996 seconds. Existing provider
failure, injected OOM, wrong-length, cancellation and ciphertext-mutation checks
remain green. The fixture directly observes dispatch-time erasure; placement of
the finally block establishes erasure before the unmodified storage call.

Android/JVM, both-ABI builds, lint, fixture isolation and alignment pass; both
native libraries are unchanged. Unsigned release is 612,647 bytes, SHA256
`93a7472e25b125b4ed6f796af4d54e1379ed21c8be428c61133129af0c661b85`.
Reviewed last-use placement, caller ownership, provider-failure cleanup, public
result lifetime and unchanged authenticated storage semantics. This covers our
managed destination, not all internal provider/VM copies. Evidence:
`.cache/android-wallet/resume-20260915/unlock-retirement/`.

2026-09-15: strengthened the existing fuzz compile-manifest gate against
sanitizer opt-outs, recovery re-enablement and coverage opt-outs. Previously an
enabling flag anywhere in the command was enough, even if a later argument
disabled the sanitizer. The baseline accepted a copied core command with
`-fno-sanitize=address`; the new regression failed on that false qualification.
The checker now tokenizes command arguments and refuses explicit opt-outs for
authored/provider compilations. Single/double-quoted flags are checked; macro
values containing similar text remain valid controls.

All 31 refusal mutations and two valid macro controls pass, using only copied
manifests. No weakened target is compiled or executed. The actual fuzz manifest
still qualifies 156 sanitized authored/provider compilations and 106 covered
library/fuzz compilations. The registered wallet_fuzz_profile test passes in
14.91 seconds, including a fresh generated positive profile and rejection of an
unsanitized configuration. Source/provider C, runtime sanitizers, assertions,
complexity caps and APK contents are unchanged. Release build, fixture isolation
and alignment pass; the unsigned APK remains byte-identical to b8bc5395c.
Reviewed token boundaries, quote handling, source scope, mutation selection and
failure diagnostics. Evidence:
`.cache/android-wallet/resume-20260915/fuzz-flag-overrides/`.

2026-09-15: the same fuzz gate now requires enabling flags to be actual compiler
arguments too. The baseline incorrectly qualified a command whose sanitizer
flag had been replaced by a macro value containing the flag's text. Exact
argument lookup closes this gap for ASan/UBSan, failure recovery and authored
integer checks; coverage is recognized only in a sanitizer argument. Quoted
real options remain accepted. All 35 refusal mutations and four positive
controls pass; the registered profile check passes in 19.54 seconds with the
same 156/106 actual compilation counts. No source, compiler command or APK
bytes change. Evidence: `.cache/android-wallet/resume-20260915/fuzz-flag-tokens/`.

2026-09-15: three public-result JNI entries now erase their native entropy and
blinding before Java allocation: receivingAddress, createWalletHeader and
recoveredWalletAddress. One cleanup path retains failure erasure; only the public
address/header survives the VM call. Derivation and authenticated recovery are
unchanged. The new boundary assertion fails on the original code, then passes
with all five entropy lengths, both networks, RNG refusal and VM fault cases.

All92 sanitizer groups pass69.07s; strict Clang/GCC source and fixture analysis
and unchanged complexity caps10/15 pass. The bounded JNI fuzzer completes77,539
cases/121s without a finding (peak63 MiB, cap512 MiB). The erasure observer runs
on x86-64 API30/35/36; ARM64 compiles. Four real-VM key/header/GCM recovery tests
pass each API in2.047/10.036/3.628s. Android/JVM, both-ABI builds, Android lint,
fixture isolation and alignment pass. Root lint has separate pre-existing
failures: empty .agents/.codex root entries and the unchanged flag-registry
selftest. Wallet validation remains scoped; no global lint success is claimed.

Unsigned release is612,807 bytes, SHA256
`c5934e13a57e290fbe0ae0973f87c68005b4a4efa7e12263f4b4dea76ede6d21`.
This bounds our native scratch lifetime, not all VM/provider copies or hardware
custody. Full review: [C_SAFETY_REVIEW.md](C_SAFETY_REVIEW.md). Evidence:
`.cache/android-wallet/resume-20260915/jni-secret-retirement/`.
Next: verify scan submission retires a claimed request when scheduling its
timeout throws, including an exception after enqueue.

2026-09-15: scan submission now retires its pending request and timeout on an
Error during timeout scheduling or Binder handoff, then propagates the original
Error. A failure while posting the failure notification cannot replace it.
Ordinary Exception/refusal handling remains false-returning, and the submitted
frame is still erased in finally on every exit.

Four new baseline tests fail because the request remains pending after an
injected OOM before/after enqueue, during decoder handoff, or alongside a
secondary notification failure. The fixed six-test fixture additionally proves
ordinary refusals and successful bounded submission, including erasure of a
competing frame. It uses a local Binder stub and private main Handler; separate
real-service tests continue to verify isolated identity and deadline behavior.
All 18 selected tests pass x86-64 API30/35/36 in 15.553/63.935/44.444 seconds.
No test deadline is relaxed; no actual OOM, camera, wallet or key is involved.

Android/JVM tests, both-ABI builds, Android lint, fixture isolation, architecture
and 16 KiB alignment pass. Reviewed atomic retirement, timeout cancellation,
exception identity, input ownership and bounded queue behavior. Native sources
are unchanged from the preceding 92-group sanitizer/fuzz/analysis checkpoint.
Fresh source-only builds in separate directories reproduce both the preceding
and current complete unsigned APKs. Both current native libraries are also
byte-identical to those from the independently rebuilt preceding commit.
This measures checkout-path independence on one host/toolchain, not cross-host
or signed-release reproduction. Existing root-lint failures remain outside this
Android-only change.

Unsigned release remains 612,807 bytes, SHA256
`767108fb3f5ca812cb46329e7705b9c89a6c543c9b8392e045e3c8cc939377ea`.
Evidence: `.cache/android-wallet/resume-20260915/scan-submit-failure/`, including
the exact source archive/patch, baseline failures and reproduction hashes.
Next: WalletAuthentication.begin currently registers its timeout outside the
protected prompt block. Its existing AuthenticationWindow tests cover C/JNI
timestamps only. Add deterministic scheduling-failure coverage for pending
prompt cleanup without requesting authentication or weakening custody policy.

2026-09-15: authentication setup now protects timeout construction/registration
as well as prompt startup. Runtime setup failures retire the pending request,
remove any queued timeout and cancel its signal. Errors perform the same cleanup
and propagate unchanged even if failure notification throws. Failure notification
for an ordinary false return stays outside the setup catch so its own exception
keeps the existing behavior. Background failure retains only a deferred failure
notification, delivered once on resume.

The new fixture fails four of six tests on the baseline because a scheduling
failure retains the pending request. Fixed coverage includes exceptions/errors
before and after enqueue, cancellation, removed deadlines, zero approvals,
secondary errors, false-post behavior and deferred notification. It uses an
unattached Activity and an uninitialized public cipher handle; no prompt,
authentication, wallet or key is opened by this new fixture. The existing native
deadline and isolated Keystore-refusal controls run alongside it. All 10 tests
pass without skips on x86-64 API30/35/36 in 1.212/6.113/2.280 seconds. The Keystore
controls use only their own fresh temporary aliases and remove them afterward.
Positive hardware-backed authentication remains unqualified.

Android/JVM tests, both-ABI builds, Android lint, fixture isolation, architecture
and 16 KiB alignment pass. Reviewed main-thread ownership, cancellation before
reporting, late-callback identity, original-error preservation, no new queues or
secret copies and unchanged per-use authentication policy. The two native
libraries remain byte-identical to the preceding validated checkpoint. A fresh
source-only build in a separate directory reproduces the complete unsigned APK;
the same-host/toolchain limit still applies. Existing root-lint failures remain.
Unsigned release remains 612,807 bytes, SHA256
`319a9b13c856ec07d11584fb0cba7eaf8d04c74d02d90d2bf91e0d5f34d3a008`.
Evidence: `.cache/android-wallet/resume-20260915/auth-timeout-failure/`.

Next: CameraCapture.start publishes its worker Handler before posting its
startup timeout/open operation. Its Error cleanup currently covers only the
pre-Handler stage, and CameraStartFailureInstrumentedTest exercises construction
and worker-start failures only. Verify failures after Handler publication using
bounded synthetic posts and real worker shutdown, without opening a camera.

2026-09-15: camera startup now closes through its published worker Handler when
an Error escapes timeout/open scheduling, then rethrows the same Error. The
pre-Handler cleanup remains intact. After Handler publication, admission is
released only by the existing worker release path, which still waits for a
pending OS open's terminal callback.

The new fixture fails both fatal-post tests on the original code; ordinary
refusals pass. It starts a real worker and rejects the timeout before/after
enqueue, then observes timeout removal, worker termination, released admission,
no automatic retry and admission of a fresh owner. It opens no camera and does
not simulate cleanup by resetting global ownership. The ordinary false/exception
controls retain one notification; fatal failures propagate without notification.

All 13 selected tests pass on each x86-64 API30/35/36 emulator: nine synthetic
ownership/dispatch tests, three actual capture/open-cancellation/lifecycle tests,
and one permission-refusal test. The actual capture groups complete in
107.481/202.591/187.922 seconds, including repeated background cleanup and explicit
restart after recreation. No deadline is relaxed and no camera test is skipped.
Android/JVM tests, both-ABI builds, Android lint, fixture isolation, architecture
and 16 KiB alignment pass. The native libraries are byte-identical to the prior
validated checkpoint. A source-only build in a separate directory reproduces
the complete unsigned release; the same-host/toolchain limit remains.

Reviewed asynchronous worker release, pending-open ownership, original-error
preservation, bounded retry behavior and unchanged frame ownership. The change
does not make framework shutdown allocation-free or qualify physical camera
interoperability. Existing root-lint failures and hardware-custody limits remain.
Unsigned release remains 612,807 bytes, SHA256
`22b13eae23a1aa81f64b59c259c6a8386b6204b4502bb752c1e654a6a73b431d`.
Exact APKs, baseline failures and reproduction evidence are preserved under
`.cache/android-wallet/resume-20260915/camera-startup-post/`.

Next: finish the controller setup-timeout failure regression with a real worker
owning only public marker entropy. Then review/reuse the earlier prepared-session
finalizer change from 6227d2d97; current debug bytecode still allocates its bound
callback inside close(), before requesting worker shutdown.

2026-09-15: setup-timeout scheduling now retires the foreground session when
the clock read or Handler post throws. Ordinary exceptions report refusal;
fatal errors close the owner before propagating the original error, including
when failure rendering also throws. A failed post that already enqueued its
timer removes that timer. Exact expiry retains its existing message and the
successful path retains the original immutable setup window.

The new regression fails four of seven tests on the original controller. It
uses an unlaunched controller on the storage-free debug host and a real worker
holding only public marker entropy. It observes session closure, timer removal,
worker termination and entropy erasure after worker release. No authentication,
key generation or storage operation occurs. All 15 selected scheduling and
worker/UI-expiry tests pass on x86-64 API30/35/36 without skips or weakened
deadlines. API30 completes in 128.104 seconds; API36's seven scheduling and eight
expiry tests complete in 165.758/82.222 seconds. The fresh API35 KVM device runs
all 15 in 13.782 seconds.

The inherited software-emulated API35 initially reports 15 passed in its device
log, but the outer command times out before collecting completion. A retry is
stopped after observing a System UI ANR obstructing lifecycle transitions. Its
reboot does not finish within the bounded readiness checks. Acceptance instead
uses a fresh isolated API35 profile, the installed image and the existing
hash-qualified child-reaping launcher with usable KVM. Initial timeout/ANR logs
remain preserved; no acceptance threshold or test timeout is relaxed.

Android/JVM tests, both-ABI builds, Android lint, fixture isolation, architecture
and 16 KiB alignment pass. The native libraries remain byte-identical. A fresh
source-only build in a separate directory reproduces the complete unsigned APK
under the same host/toolchain. Reviewed main-thread ownership, timer identity,
worker-side erasure, original-error preservation and bounded successful setup.
This does not make Android cleanup allocation-free or qualify hardware custody.
Existing root-lint failures remain. Unsigned release is 612,807 bytes, SHA256
`4acec13eb0239517f1385482dd2786662130a6c6869eef45b888a55a0835041e`.
Evidence: `.cache/android-wallet/resume-20260915/setup-timeout-failure/`;
fresh-emulator launch evidence: the sibling `api35-kvm/` directory.

Next: integrate the independently tested prepared-session finalizer, then
retire restored entropy before the public-address UI handoff.

2026-09-15: reused the prepared-session finalizer from 6227d2d9740f96cc47bab32ef0f8674bec9c3912.
The session now allocates its bound cleanup callback during empty construction,
before secret ownership or worker admission. close() selects that existing
callback before asking the worker to shut down. Active work still owns setup
until pool termination. This removes the session's close-time callback
allocation; it does not make framework shutdown allocation-free.

The original regression was adapted to the current four-argument Setup model
and forwards main-thread assertion failures to the test runner. On the baseline
it fails because no prepared callback exists. The fixed test observes callback
identity across repeated close(), retained public marker entropy during active
work, erasure after termination and removal of the pool's callback reference.
It creates no wallet, key or storage directory. Debug bytecode independently
shows close() loading finishSession without its previous callback allocation.

The finalizer and five worker-expiry tests pass on each x86-64 API30/35/36;
API30 also runs all seven controller scheduling regressions in that batch
(13 tests, 95.219 seconds). The six-test API35/36 groups take 0.057/1.683 seconds.
No test is skipped and worker/admission bounds remain unchanged. Android/JVM,
both-ABI builds, lint, fixture isolation, architecture and alignment pass.
Native libraries are byte-identical to the preceding checkpoint. A source-only
snapshot identified by tree 3a6d13cca4a8629af0afab1ff8708ed9dd573ffd reproduces
the full APK in a separate directory; the commit's app source matches that
tree except for this progress log. The same-host/toolchain limit remains.

Reviewed construction failure before admission, callback lifetime, idempotent
closure and worker-owned erasure. Hardware-custody and existing root-lint limits
remain. Unsigned release is 612,807 bytes, SHA256
`7fae1dc8bcd8d79aaa1d3f26b85f4398eae9c0259637aa75e082631630d13285`.
Fresh evidence: `.cache/android-wallet/resume-20260915/session-finalizer-reuse/`.

Next: commit the demonstrated restoration entropy retirement before UI dispatch.

2026-09-15: sealing now clears its entropy argument before returning the public
receiving address to UI scheduling. Restored entropy is a local array outside
Setup, so the previous Setup cleanup left it live until restore()'s outer
finally, after the handoff. The sealing finally now wipes that array as well.
The outer cleanup remains as coverage for earlier failures. Creation retains
its existing erasure behavior. Persistence order and restored change-state
policy are unchanged; entropy remains available throughout native use.

The existing real GCM/create/restore fixture now observes the encryption input
at the actual UI executor boundary. On the baseline, one of its eight tests
fails: restoration dispatch observes nonzero entropy. The creation control
passes. With the fix, both routes preserve the expected public address and
persisted record while dispatch observes erased entropy. Fixtures use only
fixed public entropy, key and IV in unique temporary directories, and clean
only their known files. No Keystore bypass enters production code.

All 20 sealing, worker-expiry, unlock-output and finalizer tests pass on each
x86-64 API30/35/36 device in 4.710/0.505/9.022 seconds, with no skips. Android/JVM
tests, both-ABI builds, lint, fixture isolation, architecture and alignment pass.
Both native libraries are byte-identical. Source tree
c2e798f74538a090a121dd42b370234a1abee8c7 reproduces the complete unsigned APK
from a separate source-only directory; this commit matches its app source
except for the progress log. Same-host/toolchain reproduction is the measured
claim. Reviewed last-use ordering, worker ownership, failure paths, repeated
erasure and no extra secret copies. Framework/provider copies and physical
hardware custody remain unqualified; existing root-lint failures remain.
Unsigned release is 612,807 bytes, SHA256
`385f30da47c87a76b432e5aac2d7784956352164b931d152b0e82032be59b805`.
Evidence: `.cache/android-wallet/resume-20260915/seal-entropy-handoff/`.

Next: inspect native fresh-wallet/change-state creation for secret retention
across persistence, using the existing safety and fault-injection fixtures.

2026-09-16: native fresh-wallet/change-state creation retains borrowed entropy;
its derived key and blinding scratch already clear before filesystem work.
That audit required no implementation change. Separately, inspection of the
minified release confirms the session constructor prepares its finalizer,
close() loads that field without allocating a replacement, and sealing calls
the full-byte-array wipe before both return and rethrow. The byte/character
helpers call Arrays.fill across the full array. This is DEX inspection, not
physical-memory or hardware-custody acceptance. Evidence is under
`.cache/android-wallet/resume-20260915/release-cleanup-review/`.

Both native QR decoder adapters now erase and free their pixel copy before
allocating or transferring the public Java result. The decoded text/request
owns its arrays and remains valid through transfer; its scratch then clears.
The existing fault-injection fixture now observes no native pixel owner at
NewByteArray, in addition to its existing full-erasure-before-free assertion.
That assertion fails on the original code and passes with the new ordering.
No new allocation, parser behavior or JNI interface is introduced. The complete
C hazard review is recorded in C_SAFETY_REVIEW.md.

The default safety gate passes 92 sanitizer groups, strict source/provider
analysis and unchanged complexity caps. A separate profile passes 96 groups
including its extra oracle checks. Normal/fuzz fixture analysis passes with
Clang and GCC. The existing packing-control fuzzer runs 15,569 cases in
121 seconds without a finding, with ASan/UBSan/integer checks and a 512 MiB
RSS bound (104 MiB observed); its initialization also executes the decoder
retirement regression. The emitted compiler manifest passes its sanitizer and
coverage audit. TLS remains excluded and its parked investigation is untouched.

Native fake-VM fixtures pass on x86-64 API30/35/36; both x86-64 and ARM64 fixtures
compile with the NDK. Six real Android JNI/isolated-service tests pass on each
API in 7.017/0.885/27.757 seconds. The actual image-backed camera, exact public
review and recreation/rescan journey also pass API30/36 in 69.649/103.937 seconds.
No camera fixture is skipped and no deadline is relaxed. The fresh API35 KVM
emulator completes console shutdown with its launcher returning exit 0.

Android/JVM tests, both-ABI builds, debug/release lint, fixture isolation,
architecture and 16 KiB alignment pass. A source-only build from tree
ea8ab7250030e66f76d4db73571009dc592010aa reproduces the complete unsigned APK
in a separate directory. This commit matches that app source except for the
two review logs. Reproduction remains limited to this host/toolchain. The DEX
is byte-identical to the inspected prior release. Unsigned APK size is 612,823
bytes (+16), SHA256
`718075922f289ea6dbfd41c56537801280d0a8906e2f7d8a467c1a014d1fdda0`.
Evidence: `.cache/android-wallet/resume-20260915/jni-decoder-retirement/`.
Root-lint failures, physical camera/ARM64 runtime and hardware-custody acceptance
remain unresolved. The native pixel lifetime claim does not cover all managed,
camera-driver or provider copies.

Next: audit the remaining JNI result handoffs for unnecessary native input
retention and reuse their existing fault-injection fixtures for any real gap.

2026-09-16: the remaining JNI heap-backed draft, review and sync inputs already
retire before their result handoffs. No further native change was needed.
CameraCapture now prepares its release Runnable during construction, before
worker/camera admission, and reuses it in close(). This applies the same
prepared-callback pattern as the wallet session. Handler internals may still
allocate queue messages; this is not allocation-free Android shutdown.

The added regression captures the owner's Runnable identities before start(),
then refuses the startup timer before any camera open can be queued. An
observing Handler delegates release to the real worker and records two close
requests. The old implementation fails the prepared-identity assertion in
0.254 seconds. The fix uses the same prepared callback for both requests,
terminates the worker and returns admission; a fresh owner can then enter.
Cleanup never resets the global guard or quits the worker outside its owner.
No camera, permission, wallet, key or actual memory pressure is used by this
new test.

All 12 selected tests pass on each x86-64 API30/35/36. Ten synthetic startup,
shutdown and dispatch tests complete in 1.191/0.049/2.655 seconds; actual
pending-open cancellation and public-image review/recreation/rescan complete
in 70.623/47.415/106.282 seconds. No camera test is skipped and existing
deadlines remain unchanged. The API35 run uses the isolated KVM profile with
the same public camera image and the reviewed child-reaping launcher.

Android/JVM tests, both-ABI builds, debug/release lint, fixture isolation,
architecture and alignment pass. Both native libraries remain byte-identical.
Minified DEX inspection shows the constructor storing releaseRequest and
close() loading it before Handler.post, with no new-instance in close(). A
source-only snapshot, tree 3196ff4d5fbbc4d03d0755735d0e749a41962ca4, reproduces
the complete unsigned APK in a separate directory. This commit matches that
app source except for the security/progress notes. Same-host/toolchain limits
remain. Reviewed pre-admission construction, idempotent worker release, pending
OS-open ownership, callback lifetime and unchanged frame erasure. Hardware
custody and existing root-lint limits remain unqualified/unresolved.
Unsigned release remains 612,823 bytes, SHA256
`f78b6c59bea02d1356b8b60cf0886aad11240875137ac72d69a08ba5f8197a7b`.
Evidence: `.cache/android-wallet/resume-20260916/camera-release-callback/`.

Next: verify scanner Activity cleanup still attempts its decoder and preview
when one component's close or clear operation throws.

2026-09-16: scanner Activity shutdown now attempts all three independent owners:
camera, isolated decoder and preview. Previously a camera close exception could
skip the live decoder binding and preview; a decoder unbind exception could
skip the preview. The first failure propagates after all attempts, without
allocating suppressed-exception storage. Failed camera/decoder references remain
available for a later lifecycle cleanup; successful closes release references.
Pause and destruction call their Android superclass cleanup in finally blocks.
All stop paths now explicitly retire the preview as well.

Five new emulator-only tests use an actual isolated decoder binding, a public
bitmap and an unstarted CameraCapture with a private fault-injecting Handler.
They exercise Exception/Error before and after release enqueue, failure after
actual unbinding, secondary unbind/view-clear errors, and an ordinary-pause
control. Four of five fail on the original implementation (5.103 seconds).
The fixed tests observe decoder retirement/unbinding, queued-packet erasure,
blackened preview pixels, inert late frame callbacks, retained failed owners
and original-error identity. No camera/worker admission is forged or reset;
no wallet, key, seed, storage operation or actual memory pressure is involved.

All seven selected tests pass on each x86-64 API30/35/36, with no skips. The five
cleanup tests take 78.457/6.257/146.517 seconds; actual pending-open cancellation
and public-image review/recreation/rescan take 70.488/47.143/95.926 seconds.
Android/JVM tests, both-ABI builds, debug/release lint, fixture isolation,
architecture and alignment pass. Both native libraries remain byte-identical.
Minified DEX inspection confirms the three cleanup catch paths, retention of
the first error and superclass calls on normal/exceptional pause/destruction.

A source-only build from tree 43bdfb2693e9b35c7a6d52518916fdca447482ec reproduces
the complete unsigned APK in a separate directory. This commit's app source
matches that tree except for security/progress notes. Same-host/toolchain
reproduction does not qualify physical hardware, custody or all Android/driver
copies. Existing root-lint failures and parked TLS limits remain. The release
remains 612,823 bytes, SHA256
`e1713f307ebec4affe7b870aaeec4f4c736eb3be83039a701e17938e12d77c19`.
Evidence: `.cache/android-wallet/resume-20260916/scanner-cleanup-failure/`.

Next: check whether the decoder service retires its managed camera frame before
handing the decoded result back through Binder; preserve its single-input bound
and queued-input cancellation cleanup.

2026-09-16: the decoder service now clears its managed camera frame immediately
after native decoding, before reply marshalling can allocate or block. Its
single-input busy flag remains held through the reply, decoded text is cleared
afterward, and OwnedExecutor cleanup still clears queued/rejected inputs.
The native decoder already retired its separate pixel copy before VM output.

A local service endpoint fixture uses the actual worker and packaged JNI with
a public QR image. Its reply holds the handoff open, directly observing frame
retirement, intact response text, rejection/clearing of a competing input and
text erasure when the handoff ends. Wrong-network decoding and a reply exception
exercise refusal cleanup; queued cancellation delivers nothing and clears its
frame. Three of four tests fail on the original service at the frame-retirement
assertion (0.339 seconds). An initial fixture return-type error was corrected
before that baseline; its runner diagnostic remains in the evidence directory.
This local endpoint does not claim Binder isolation.

All eight selected tests pass on each x86-64 API30/35/36 in
6.270/0.992/25.923 seconds, with no skips: the four ownership tests, three actual
JNI QR tests and the real isolated-service identity/network round trip.
Android/JVM tests, both-ABI builds, debug/release lint, fixture isolation,
architecture and alignment pass. Native libraries are byte-identical to the
previous tested release. Minified worker DEX calls the whole-array erasure
helper before each reply path; the returned text remains live only through
its handoff and retains normal/exceptional cleanup afterward.

A separate source-only build from tree 1e1ce5c864a6ba4f1c8feb68c14c4380ce6803ac
reproduces the full unsigned APK. This commit matches that app source except
for security/progress notes. The release remains 612,823 bytes, SHA256
`412690bf65833c306ae3b8a8ce7504820b98e38aaa3eca11644fa1de9c107007`.
Evidence: `.cache/android-wallet/resume-20260916/service-frame-retirement/`.
Reviewed array ownership/aliasing, native-return ordering, reply exceptions,
worker cancellation, the unchanged admission bound and no new runtime
allocation. Reproduction remains same-host/toolchain; physical hardware,
custody, existing root-lint failures and parked TLS remain unqualified.

Next: continue the owned-secret handoff audit through recovery submission and
screen cleanup, reusing existing fixtures when a concrete gap is found.

2026-09-16: recovery display and keyboard cleanup now erase owned characters
before Android visibility/text operations. Previously their finally blocks
retired characters only after those operations returned or threw. The owned
buffers have no remaining use during explicit cleanup; moving the wipe earlier
also covers a stalled framework call without adding an allocation. Transfer
still preserves the input long enough to produce its owned destination.

Two new tests observe actual TextView clear callbacks, checking the owned
arrays are already zero and the text concealed before framework clearing. Both
fail on the original implementation (0.078 seconds). The fixed tests include
ordinary clears, Exception and Error, and preserve original error identity.
All 24 selected tests pass on each x86-64 API30/35/36 in
46.947/3.692/91.695 seconds: concealment/retirement, saved-state refusal, bounded
keyboard/transfer, delayed delivery and backup-screen construction failures.
Public markers and the storage-free display host only; no real seed, key,
wallet or actual memory pressure. No selected test is skipped.

Android/JVM tests, both-ABI builds, debug/release lint, fixture isolation,
architecture and alignment pass. Native libraries remain byte-identical.
Minified DEX inspection confirms each clear calls the full char-array erasure
helper before setVisibility and setText. A separate source-only build from
tree 1549afafa8d007635ddfb7fcfb86f0bc8cc7b494 reproduces the complete unsigned
APK; this commit matches that app source except for security/progress notes.
The release remains 612,823 bytes, SHA256
`1461305ec4d515011000cf85d40208086f9038d8b086cf9754bcee9599e04985`.
Evidence: `.cache/android-wallet/resume-20260916/secret-clear-retirement/`.
Reviewed replacement ownership, buffer bounds, exception propagation and main-
thread ordering. Framework/provider/GPU copies remain outside this erasure
claim. Same-host reproduction, physical-device/custody, root-lint and parked
TLS limits remain unchanged.

The existing C camera/QR fuzzers already vary pixel content. The JNI camera
fuzzer varies packing geometry/VM faults but only runs fixed decoder fixtures
at startup. Next: extend that existing target to vary decoder packet bytes
alongside VM/allocation faults, retaining exact C-to-JNI result checks.

2026-09-16: expanded the existing JNI camera fuzz target with a decoder mode:
four bounded controls followed by actual packet bytes. It varies valid/invalid
networks, VM call failure, allocation refusal, pending/null entry and partial
read/write lengths. Exact C decoder output is the transport oracle; input
immutability, complete output/tail/canary checks, full native erasure and release
before VM result allocation remain mandatory. Existing 13-byte packing inputs
retain their interpretation. The registered test adds deterministic control,
partial-transfer, malformed-header and length cases.

A temporary wrong-network JNI mutation in ignored scratch forces every decoder
request to mainnet. The old fixture accepts it; the final expanded fixture
rejects it with the expected assertion. No production source was mutated.
The reference is not independent QR recognition; existing C camera/QR fuzzers
retain that separate role. The per-hazard source review is in C_SAFETY_REVIEW.md.

The first two fuzz attempts timed out while running the complete added fixed
matrix as the first empty input. An exact full-buffer guard comparison replaces
an equivalent per-byte loop; the full matrix remains under the registered
15-second test deadline. Its cases seed separate fuzz invocations under the
unchanged five-second input limit. The final campaign completes 11,564 executions
in 121 seconds with no finding, 264 added units and peak RSS 260 MiB against a
512 MiB cap. Maximum admitted fuzz length is 147,465 bytes. Both timeout logs
and empty inputs are preserved rather than classified as decoder findings.

The final default C safety run passes 92 sanitizer groups (69.90 seconds),
Clang/GCC source/provider analysis and production/test complexity caps 10/15.
Both fixture modes separately pass Clang/GCC analysis. The affected profile with
additional integer checks passes (1.47 seconds). Actual compile commands qualify
sanitizers/fail-on-finding on 156 authored/provider compilations and fuzz
coverage on 106 library/fuzz compilations. Strict NDK builds pass for x86-64 and
ARM64; exact-hash transferred x86-64 fixtures pass on API30/35/36. The new
fuzz_decode_case frame is 1640/1728 bytes on x86-64/ARM64. No ARM64 runtime claim.
Architecture passes; pre-existing root-lint failures remain unresolved.

Only tests and review notes change. A source-only build from tree
fa7ef35a550f92e7a24f8404ccdaf091bb152353 reproduces the prior 612,823-byte unsigned
APK exactly; the commit matches that app source except this progress note.
SHA256 `1461305ec4d515011000cf85d40208086f9038d8b086cf9754bcee9599e04985`.
Evidence: `.cache/android-wallet/resume-20260916/jni-decoder-fuzz/`.
Hardware custody, physical-camera acceptance and the parked TLS candidate remain
unqualified. Next: reconcile stale custody process/deadline documentation with
the actual manifest and setup clock, then check the build enforces those bounds.

2026-09-16: reconciled custody process/deadline documentation with the actual
manifest and SetupWindow. Wallet activity work is serialized within the UI
process; QR decoding uses a separate isolated UID and creates no wallet session.
Setup uses an immutable elapsed-clock origin after authentication, rather than
counting foreground time. Explicit view cleanup retires owned characters before
calling Android. The corresponding wrapping-key source comment is corrected
without changing executable behavior or line count.

The existing checkFixtureIsolation gate now examines both merged APK manifests.
It requires one private isolated decoder at :qrdecode, stopWithTask, one private
scanner Activity and the public wallet launcher. Process/isolation declarations
outside the decoder, shared UIDs and multiprocess providers fail closed. The
check accumulates a component's attributes before evaluating it, so their order
is not authoritative. It complements actual Binder UID tests and does not prove
hardware custody or protect against a compromised OS.

The prior checker accepts shared-decoder, exported-decoder and wallet-process
mutations in both variants. The new regression passes actual APKs and both
reordered-attribute controls, and rejects all 32 new process mutations plus the
existing eight host and two asset mutations. Faults affect aapt2 output only;
no application manifest/APK is modified. Shell syntax, full Android/JVM tests,
both-ABI builds, debug/release lint, fixture isolation, native alignment and
architecture pass. Debug, test and release APKs are each byte-identical to the
previous device-qualified checkpoint. The unsigned release remains 612,823 bytes,
SHA256 `1461305ec4d515011000cf85d40208086f9038d8b086cf9754bcee9599e04985`.
Evidence: `.cache/android-wallet/resume-20260916/manifest-process-boundary/`.
No new runtime route, key operation or custody qualification. Existing root-
lint failures, physical-hardware acceptance and parked TLS limits remain.

Next: inspect whether fuzz manifest qualification distinguishes actual test
object outputs from test-shaped text in unrelated compiler arguments.

2026-09-16: fuzz compile qualification now determines the standalone-test
coverage exemption from the actual compiler output operand. Previously,
test-shaped text anywhere in a command could exempt a library compilation.
The new regression replaces a library coverage flag with a harmless macro
containing CMakeFiles/pretend_tests.dir/: the old checker accepts it and reports
105 covered compilations instead of 106. Only copied command manifests are
mutated; no weakened target is built or executed.

The checker consumes the emitted separated -o argument and requires exactly one
nonempty output. Output names do not count as enabling sanitizer flags. Five
new negative cases cover plain/quoted macros, include paths, duplicate output
and missing output; two positive controls retain real coverage alongside a
test-shaped macro and quote the actual -o option. Existing sanitizer, integer,
coverage, opt-out and empty-scope mutations remain enforced.

The registered wallet_fuzz_profile passes (24.06 seconds), as does the separate
final mutation run. The actual configured profile still qualifies 156 authored/
provider sanitizer compilations and 106 library/fuzz coverage compilations.
Architecture and whitespace checks pass. This changes only CMake qualification
and regression scripts; the previously qualified product APK is unchanged.
Evidence: `.cache/android-wallet/resume-20260916/fuzz-output-classification/`.
Existing root-lint failures, hardware acceptance and parked TLS limits remain.
Next: check whether actual fuzz harness translation units, as well as linked
authored/provider sources, receive the intended instrumentation qualification.

2026-09-16: the fuzz gate now also qualifies every native test translation unit
compiled into a fuzz target, using its actual output operand. The old checker
accepted a copied manifest with coverage removed from fuzz_blake2's harness.
Harnesses and helpers now require ASan/UBSan, fail-on-finding and coverage flags,
and reject overriding opt-outs. Standalone tests/seed writers remain outside
this added scope; filenames shared with fuzz targets do not grant an exemption.
An empty harness scope fails. Separate oracle libraries are not included in
this harness count, and this is compile qualification rather than runtime proof.

Ten added negative mutations cover three harness/helper source forms and an
empty harness scope. Five positive controls cover quoted real options and the
standalone/seed distinction. Existing mutations remain enforced. The first
registered run exceeded its unchanged 60-second mutation deadline; the complete
standalone suite passed. Caching manifest entries and parsing each command's
JSON object once removes repeated large-array parsing. The final registered
profile passes in 28.51 seconds, with 156 authored/provider and 70 harness/helper
compilations and coverage on 176 library/fuzz compilations. The oracle-enabled
configuration and final mutation suite also pass: 156 authored/provider plus 75
harness/helper compilations, with 181 covered library/fuzz compilations.

Architecture and whitespace checks pass. No compiler configuration, C source,
test deadline, product artifact or custody authority changes. Evidence, including
the initial timeout: `.cache/android-wallet/resume-20260916/fuzz-harness-qualification/`.
Hardware acceptance, pre-existing root-lint failures and parked TLS remain
unresolved. Next: check whether JNI mnemonic conversions retain consumed input
scratch while transferring their secret result to its preowned Java destination.

2026-09-16: JNI mnemonic generation now clears its consumed entropy before the
VM transfers the phrase; restoration clears its consumed byte phrase before
transferring entropy. Necessary result scratch retains its existing ownership
and post-transfer wipe. Failure paths use the same cleanup and never publish a
result after a failed conversion. Caller arrays and the JNI ABI are unchanged.

The fake VM observes full consumed-input erasure before normal, refused and
partial output transfers. A fixture-only wrapper tracks restoration's byte text
while calling the real decoder. The old generation path fails the new boundary
assertion; after fixing only generation, restoration fails it too. The complete
fix passes the focused additional-integer sanitizer profile (0.53 seconds), all
92 default sanitizer groups (79.05 seconds), Clang/GCC source/provider and both
fixture-mode analysis, and unchanged production/test complexity caps 10/15.
The existing seeded JNI fuzzer completes 81,793 inputs in 121 seconds without a
finding, with a five-second input limit and 512 MiB RSS cap (59 MiB observed).
Actual compile commands qualify 156 authored/provider and 75 harness/helper
compilations, with 181 covered library/fuzz compilations.

Strict NDK erasure fixtures build for x86-64 and ARM64. Exact-hash transferred
x86-64 fixtures pass API30/35/36, and four actual JNI/public GCM-vector tests pass
on each in 2.013/0.146/3.607 seconds with no skips. Android/JVM tests, both-ABI
builds, debug/release lint, fixture isolation, alignment and architecture pass.
The source-only tree e5d108e96a16e0e13624ddfd4963203493e602e5 reproduces the
unsigned APK byte-for-byte; committed app source matches except this progress
note. Release is 612,839 bytes (16 bytes larger), SHA256
`93476511db12f12cf91db6c9beb40392f51129d42b66b630bc8b798ecad75474`.
This is same-host/toolchain reproduction and emulator evidence, not ARM64
execution or hardware custody acceptance. Full hazard review is recorded in
C_SAFETY_REVIEW.md. Evidence:
`.cache/android-wallet/resume-20260916/jni-mnemonic-retirement/`.
Existing root-lint failures and parked TLS remain unresolved. Next: inspect
managed record/key result ownership around view and platform handoffs.

2026-09-16: the managed record/key handoff inspection found no additional leak
in the inspected paths. The JNI key fuzzer did not vary partial output lengths:
only its separate fixed test selected them. An unused control bit now selects
a 0..255-byte/character prefix from the third input byte when present. Existing
two-byte inputs retain their behavior, and the 217-byte input bound is unchanged.
Prefix mode can combine with existing malformed-input/VM/RNG controls; its third
byte may also be payload, so those values are not universally independent.

The registered test now calls the same fuzz-input function and checks 33 cases
across entropy creation, phrase generation and restoration. It observes actual
copied counts, exact public prefix bytes/characters, untouched destination tails,
pending exceptions, caller input immutability and native erasure. The new test
fails on the prior full-copy-only behavior, then passes with prefix selection.
Both normal/fuzz fixture modes pass Clang/GCC analysis; all 92 default sanitizer
groups pass (78.76 seconds), as does the focused additional-integer profile
(0.51 seconds). Existing complexity caps 10/15 pass; the shared fuzz-input
function reaches the test cap of 15. Strict NDK fixtures compile for both ABIs
and pass after exact-hash transfer to API30/35/36 x86-64 emulators. The seeded
campaign completes 78,258 inputs in 121 seconds without a finding, with nine
added units and peak RSS 60 MiB; limits remain five seconds/input and 512 MiB.
Actual compile qualification remains 156 authored/provider plus 75 harness/
helper compilations and 181 library/fuzz coverage compilations.

This slice changes only the fixture and review notes. Architecture and whitespace
checks pass. The product source and previously reproduced 612,839-byte release
APK remain unchanged, SHA256
`93476511db12f12cf91db6c9beb40392f51129d42b66b630bc8b798ecad75474`.
The mnemonic erasure order was additionally inspected in both packaged ABIs:
their .text sections match the corresponding symbol-bearing build exactly, and
the consumed-input wipe calls precede the JNI output calls. This supplements
the runtime observer and is not an ARM64 execution claim. Disassembly evidence
is in the preceding jni-mnemonic-retirement lane; this fixture's full per-hazard
review is in C_SAFETY_REVIEW.md, with execution evidence under
`.cache/android-wallet/resume-20260916/jni-key-partial-fuzz/`.
Hardware custody, physical camera acceptance, parked TLS and pre-existing root-
lint failures remain unqualified. Next: inspect native operation ownership and
publication guards for stale UI or JNI calls after a foreground owner closes.

2026-09-16: native sync/review inspection found no stale-handle defect in the
inspected paths. Added a registered concurrent JNI sync-owner fixture to test
the existing lock/ID invariant directly. In sixteen bounded cycles, two callers
query an owner while the main thread closes/replaces it. A barrier then requires
all retired snapshots, requests and failure callbacks to return cancellation.
The replacement has a new owner ID but the same attempt token and must remain
active with its first request unconsumed. Each thread owns its fake VM result;
no shared fake exception/array state hides a native registry race.

The additional-integer ASan/UBSan fixture passes in 0.04 seconds. A separate
ThreadSanitizer profile passes in 0.07 seconds; emitted commands for all 81 built
objects contain thread/fail-on-finding flags. An overlapping-write detector
control reports a race and its atomic counterpart passes. Two initial short
controls returned without reports; their cause remains unclaimed. A temporary
JNI source copy with no-op lock substitutions triggers a data-race report in
registry lookup/publication and exit 66. Production source and global ASLR
settings remain unchanged. The documented README command uses a separate host
build, TLS off, a fifteen-second registered deadline and explicit report/exit
options. This qualifies observed C-registry interleavings, not every schedule
or a real VM's concurrency.

The full C safety run passes 93 sanitizer groups (79.55 seconds), Clang/GCC
source/provider and fixture analysis, and unchanged 10/15 complexity caps.
Strict native fixtures build for both ABIs; exact-hash transferred x86-64
fixtures pass API30/35/36. ARM64 is compile-only. The enabled fuzz profile now
qualifies 158 authored/provider plus 75 harness/helper compilations, with 181
library/fuzz coverage compilations. Release build, APK isolation/alignment,
architecture and whitespace gates pass.

A fresh source-only build from tree 203bfe560ed6c9788756aa9ffe7670c9ed2c4d7a
reproduces the existing 612,839-byte unsigned APK exactly. The committed app
source matches except this progress note; no product C or Kotlin changes.
SHA256 `93476511db12f12cf91db6c9beb40392f51129d42b66b630bc8b798ecad75474`.
Evidence: `.cache/android-wallet/resume-20260916/jni-owner-races/`.
Full hazard review is in C_SAFETY_REVIEW.md. Existing root-lint failures,
hardware custody, physical-camera acceptance and parked TLS remain unresolved.
Next: inspect setup/foreground expiry decisions when queued UI work or provider
work completes after its original time window.

2026-09-16: foreground closure now refuses new persistence after provider work.
Inspection confirmed the existing elapsed-window checks cover delayed UI,
worker and provider completion. A separate cancellation gap remained: closing
while GCM was running still allowed create/restore persistence or authenticated
pending-record promotion after GCM returned. Three public-vector regressions
reproduced those effects on unchanged production code; a live pending-unlock
control passed. No hardware key, prompt, real seed or funded record was used.

The platform worker now reads its existing atomic closed state immediately
before each persistence call. That read is the admission point: closure before
it refuses the new operation; closure afterward lets C finish its existing
bounded durability protocol. Active provider input remains worker-owned until
return, then clears through existing finally blocks. Closed callbacks remain
inert. Tests hold real software GCM completion behind a bounded latch, close on
the main thread, observe that the active input remains intact, then verify full
cleanup and absence of new storage or exact retention of the pending record.
The live control still promotes precisely the authenticated bytes.

All 27 setup/seal/unlock/expiry/session-close tests pass without skips on API30,
API35 and API36 x86-64 emulators in 45.262/3.116/75.887 seconds. Android/JVM tests,
both-ABI builds, debug/release lint, APK fixture isolation/alignment, architecture
and whitespace checks pass. Both packaged native libraries are byte-identical
to the previous release; C source, cryptography and storage protocols did not
change. These public fixtures do not qualify hardware custody or ARM64 execution.

Source-only tree 32e97e2b964ab8a59eb715e69d95b106e57825e6 reproduces the complete
unsigned release APK exactly from a fresh directory on the same host/toolchain.
The APK remains 612,839 bytes; SHA256 is
`ab82b4690fa9990cc183301222e9aae1d187d789e6b92941caf8a96257e74a78`.
Committed app source matches that tree except this progress note. Baseline/final
APKs, failing/passing tests, native comparisons and reproduction evidence are
under `.cache/android-wallet/resume-20260916/session-persistence-cancel/`.
Hardware custody, physical-camera acceptance, parked TLS and prior root-lint
failures remain unresolved. Next: inspect private-key scratch retirement in the
existing offline signing primitive; no send or broadcast authority is added.

2026-09-16: the offline signing primitive now retires consumed inputs before
public encoding and verification. Raw context-blinding bytes clear immediately
after initialization, and the private-key copy clears after signing. Low-S
normalization is still the next provider operation; it now runs in the encoding
helper after that wipe. Final whole-work and EC-context cleanup remain intact.
The digest stays available for verification. No signing JNI/UI route, nonce
policy, signature bytes, custody gate or transaction authority changes.

The existing provider-failure fixture now observes both earlier retirements.
Unchanged code fails the blinding assertion; a blinding-only change then fails
the scalar assertion. All twenty final fault modes pass, including partial RNG,
context, signing, nonce, encoding and verification failures. The observer uses
live arguments and retires pointers before return. Four focused additional-
integer sanitizer/oracle groups pass in 7.14 seconds; full C safety passes 93
registered groups in 79.63 seconds, Clang/GCC source/provider analysis and 10/15
complexity caps. Separate analysis also passes for the changed fault fixture.

The OpenSSL-enabled signature fuzzer completes 8,095 executions in 121 seconds
without a finding, with a 66-byte input cap, five-second per-input limit and
512 MiB RSS cap (266 MiB observed). Actual compile flags qualify 158 authored/
provider and 75 harness/helper compilations, with 181 coverage compilations.
Strict native real-provider and fault fixtures build for both ABIs and pass
following exact-hash transfer to API30/35/36 x86-64 emulators. ARM64 remains
compile-only. Both optimized release-archive objects preserve the wipe order.

Release build, APK fixture isolation/alignment, architecture and whitespace
checks pass. Source-only tree 10a2ff537469b57fe54031abbed263e4b9e7dd42 reproduces
the unchanged 612,839-byte unsigned APK exactly on this host/toolchain, SHA256
`ab82b4690fa9990cc183301222e9aae1d187d789e6b92941caf8a96257e74a78`.
This internal signer is absent from the packaged JNI library. Committed app
source matches except this progress note. Full per-hazard review is in
C_SAFETY_REVIEW.md; evidence is under
`.cache/android-wallet/resume-20260916/signature-retirement/`.
Hardware custody, physical cameras, parked TLS and prior root-lint findings
remain unresolved. Next: inspect retirement of derived private-key scratch
before public receiving/change-address encoding.

2026-09-16: final receiving/change private-key scratch now clears before public
address hashing. The previous order retained the local scalar and chain code
through SHA256, RIPEMD160 and address encoding. Moving its existing full-object
wipe immediately after public-key derivation removes that lifetime without
adding a provider call, allocation or wipe. Borrowed seed and entropy retain
their existing owners, including the seed reused by recovered-change derivation.

The existing secret-failure fixture now captures the final key's integer identity
through a test-only linker wrapper and checks its full live erasure before public
hashing. The original implementation fails that ordering assertion. Both chains
exercise success, public-key failure and SHA256/RIPEMD160 failure; caller output
and length remain unchanged on failure. Three focused additional-integer
sanitizer groups pass in 4.95 seconds, including independent OpenSSL derivation
comparisons. Full C safety passes 93 groups in 79.62 seconds, source/provider
Clang/GCC analysis and unchanged 10/15 complexity caps. The changed fixture also
passes both analyzers and strict NDK compilation for both ABIs.

The native lifetime fixture passes after exact-hash transfer to API30/35/36
x86-64 emulators. Their four actual packaged JNI/GCM tests also pass without
skips in 2.007/0.158/3.73 seconds. Both packaged native .text sections match the
symbol-bearing libraries exactly; disassembly confirms the 64-byte private-key
wipe precedes public SHA256. The JNI key fuzzer completes 73,779 inputs in 121
seconds without a finding (max_len217, timeout5, RSS cap512 MiB, observed60 MiB).
Actual compile qualification remains 158 authored/provider plus 75 harness/helper
compilations and 181 library/fuzz coverage compilations. These claims do not
qualify hardware custody or ARM64 runtime behavior.

Android/JVM tests, both-ABI builds, debug/release lint, APK fixture isolation/
alignment, architecture and whitespace checks pass. Source-only tree
82128500cc769f3230e246b192cc8e77809ec4d5 reproduces the unsigned APK exactly on
this host/toolchain. The APK is 612,855 bytes (+16); SHA256 is
`3a3e4047142e98f8a9f15172bc672278be7b333127635c303e3bce2bf8b35a65`.
Committed app source matches except this progress note. Full per-hazard review
is in C_SAFETY_REVIEW.md; baseline/final fixtures, logs and reproduction evidence
are under `.cache/android-wallet/resume-20260916/address-key-retirement/`.
Hardware custody, physical-camera acceptance, parked TLS and prior root-lint
findings remain unresolved. Next: inspect native regression-runner deadlines so
fault or cleanup regressions cannot leave the safety acceptance running forever.

2026-09-16: every registered native host test now has an execution deadline.
The new metadata checker first rejected the unchanged registry because
wallet_receive_qr had no timeout; secret-cleanup, RNG and key-failure fixtures
also lacked limits. CMake now assigns 60 seconds only where no limit was set,
preserves explicit limits, and refuses nonpositive/nonintegral source settings.
A comparison against the original generated registry confirms all 79 existing
limits are unchanged and 14 previously unbounded tests received the fallback.

Two registered checks inspect the actual CTest registry and qualify the checker
with empty/missing/zero/negative fixtures plus a positive control. A separate
one-second test deadline must terminate a deliberately stalled five-second
fixture, with a bounded outer runner. The checks themselves have fifteen-second
limits. These gates bound fixture execution; they make no new provider, storage
latency or Android runtime guarantee and do not change production code.

The wallet's default sanitizer suite passes 95 native CTest cases in 81.54 seconds.
Both new checks also pass in the integer-sanitizer and fuzz/oracle configurations
(99 registered tests each) and the thread-sanitizer configuration (94). Only the
two metadata/termination groups ran in those additional configurations; this is
not a claim that all configured ThreadSanitizer binaries were built or executed.
Release build, fixture isolation, native alignment, architecture and whitespace
checks pass. No new C implementation or C analysis claim is involved.

Source-only tree d4d4386817ca6c7d8be13c16b6530ac1743f7e0a reproduces the existing
612,855-byte unsigned APK exactly on this host/toolchain, SHA256
`3a3e4047142e98f8a9f15172bc672278be7b333127635c303e3bce2bf8b35a65`.
Committed app source matches except this progress note. Baseline registry,
negative result, preservation comparison and execution/reproduction logs are in
`.cache/android-wallet/resume-20260916/native-test-deadlines/`.
The one-off comparison initially lacked CMake's IN_LIST policy; that diagnostic
and the corrected version declaration are retained. Hardware custody, physical
camera acceptance, parked TLS and prior root-lint findings remain unresolved.
Next: inspect ciphertext/pending-record ownership across provider and JNI error
returns, keeping real-device custody qualification separate.

2026-09-16: interrupted paired creation now reports orphan change state before
Android offers fresh setup. Creation already refused an existing .change.index;
wallet reads incorrectly returned NOT_FOUND when that was the only remaining
artifact. After both wallet records are absent, the read now checks the change
name under the existing lock and returns ALREADY_EXISTS without publishing
outputs. Truly empty storage still returns NOT_FOUND. Existing wallet/pending
priority, authentication and creation refusal remain unchanged; no repair,
deletion or overwrite path is added.

The original source fails the new orphan-status assertion. Native cases cover
empty, partial and complete initial state, unchanged outputs/files, dangling
symlinks and FIFOs. Crash/fault tests now require the stronger classification.
Three focused additional-integer sanitizer groups pass in 0.52 seconds; full C
safety passes all 95 groups in 81.06 seconds, source/provider Clang/GCC analysis
and unchanged 10/15 complexity limits. Changed fixtures separately pass both
analyzers and strict NDK compilation for both ABIs. The storage fuzzer now varies
orphan contents, sizes, capacities and link aliases: 303,995 inputs in 121 seconds
without a finding, max_len142, five-second per-input limit, 512 MiB RSS cap and
77 MiB observed. Compile qualification observes 158 authored/provider plus 75
harness compilations, with 181 coverage compilations.

Android/JVM tests, both-ABI builds, debug/release lint, APK fixture isolation/
alignment, architecture and whitespace checks pass. An existing JVM assertion
initially expected the old NOT_FOUND result; it now requires ALREADY_EXISTS and
no returned record. All four packaged JNI/storage/GCM tests pass without skips
on API30/35/36 in 3.15/0.278/8.215 seconds. The scratch verification script
initially expected five tests; inspection of source and named start/completion
records confirms four, and the corrected log verifier passes without rerunning
or discarding results. The full native storage fixture cannot finish under the
emulator shell UID: FIFO creation fails on all three, and an API35 probe reports
Permission denied. Host assertions and device policies remain unchanged; this
is not a full native-device fixture pass. ARM64 runtime and hardware custody
remain unqualified.

Source-only tree 3e4f13ff7ba43bc0bdc55671cb0d9bde94157904 reproduces the unsigned
release APK exactly on this host/toolchain: 612,903 bytes (+48), SHA256
`6c5d0c5f61ae9cd762f43df4fed9520ea05094dfd561c0b2f6715a8646f83f01`.
Committed app source matches except this progress note. The explicit 18-hazard
review is in C_SAFETY_REVIEW.md; baseline, faults, fuzz, build, device and
reproduction evidence is under
`.cache/android-wallet/resume-20260916/orphan-storage-read/`.
Hardware custody, physical-camera acceptance, parked TLS and prior root-lint
findings remain unresolved. Next: inspect recovery admission when pending and
committed artifacts coexist or metadata checks fail.

2026-09-16: storage admission now has a real-view integration regression through
the packaged native reader, WalletPlatformSession and MainActivity controller.
The six cases cover empty storage, orphan change entries of 0/40/80 bytes,
corrupt committed data alongside a complete pending record, pending unlock
admission, unsafe record permissions, and a queued setup callback delivered
after session closure. Failure checks require the storage message, absent
create/restore/unlock controls, a retired session and unchanged fixture files.
The complete-pending control checks that displaying Unlock does not promote it.

The controller is unattached and uses the existing debug display host. Every
read uses an exclusively created cache directory; no wallet-v1, prompt,
Keystore alias, real seed or authentication bypass is involved. A one-entry
queue intercepts only UI scheduling. Queue arrival and worker termination each
have five-second limits; cleanup waits for the owned worker and removes only
fixed fixture names. The separate record fixtures exercise real software GCM;
the new controller fixture uses explicitly inert ciphertext and makes no GCM or
hardware-custody claim.

Against the saved pre-fix APK, the initial five-case version fails exactly the
orphan UI assertion while four controls pass. A sixth permissions case was then
added. The fixed APK passes all 18 selected UI/storage/recovery/GCM tests on
API30/35 without skips in 88.065/7.919 seconds. The API36 combined observation
reaches its 180-second host limit after 17 unique successful completions,
including all six new UI cases. Device process inspection then confirms the
target has ended before retry; the remaining record test passes separately in
2.956 seconds. Logs verify six UI, eight recovery and three storage completions
plus that separate final result. The interrupted run is not claimed as a
successful batch, and its output and process inspection are preserved.

Android/JVM tests, both-ABI builds, debug/release lint, fixture isolation,
alignment, architecture and whitespace checks pass. No production source or
C code changes. Source-only tree 9bfa18566f13d3c9ed02480c9c6689d34576e403
reproduces the unchanged 612,903-byte unsigned release APK on this host/toolchain,
SHA256 `6c5d0c5f61ae9cd762f43df4fed9520ea05094dfd561c0b2f6715a8646f83f01`.
Committed app source matches except this progress note. Evidence is under
`.cache/android-wallet/resume-20260916/storage-admission-ui/`.
Hardware custody, physical-camera acceptance, the shell-UID FIFO fixture,
parked TLS and prior root-lint findings remain unresolved. Next: inspect native
authentication-result publication and its fail-closed cleanup boundaries.

2026-09-16: optimized GCC sanitizer execution now participates in the regular
C safety command. The previous command ran GCC analysis but executed only the
Clang suite. A separate Debug/-O2 GCC 14.2 profile, with assertions, ASan/UBSan,
leak detection, fail-on-finding and existing provider -Os settings retained,
exposed four fixture frame failures (4480/4144/4816/4256 bytes). Independent
orphan checks now have their own registered executable; reservation/wire checks
remove redundant scratch, and ownership checks use a typed dispatch table.
Every original case and complete output-preservation assertion remains.

The first complete GCC run then failed two storage fault assertions and the
combined recovery deadline. Optimized glibc used __read_chk instead of read,
bypassing the test wrapper. The shared fault harness now covers fortified read
and pread with the original capacity and bounds trap retained. A direct public
fixture checks both entries in all six fault modes and observes all twelve
required oversized-call SIGABRT results, with core dumps disabled. The two
original failing assertions now pass unchanged. Recovery's complete 160-length
and 160-byte corruption matrices run independently with their original
30-second deadlines; custody, authenticated-predecessor, network/entropy and
capacity cases remain in the bounds group. Focused GCC times are
1.78/11.95/17.60 seconds for bounds/partial/corrupt.

The revised safety command passes source/provider Clang/GCC analysis, unchanged
10/15 complexity caps, all 98 Clang groups (82.12 seconds) and all 97 optimized
GCC groups (123.25 seconds). The separate GCC OpenSSL/libsodium-oracle profile
passes all 101 groups in 131.45 seconds. Final orphan success-message error
checking is rebuilt and passes separately in all three profiles. Seven changed
C fixtures also pass both analyzers and strict x86-64/ARM64 NDK compilation;
this adds no device execution claim. Actual compile-command qualification
observes 146 optimized sanitizer compilations, including thirteen compilations
of those seven fixture sources. The largest reported changed-fixture GCC
sanitizer stack use is 3984 bytes; NDK maxima are 2296/2352 bytes on x86-64/ARM64.
The qualifier initially found the ordinary storage-fault target lacked the
frame gate; it now also enforces 4096 bytes. No warning or assertion is waived.

Android/JVM tests, both-ABI builds, debug/release lint, fixture isolation,
alignment, architecture, shell syntax and whitespace checks pass. Source-only
tree 94ff5b26c210778fab50c4df98c11a8cee47b882 reproduces the unchanged unsigned
release APK on this host/toolchain: 612,903 bytes, SHA256
`6c5d0c5f61ae9cd762f43df4fed9520ea05094dfd561c0b2f6715a8646f83f01`.
Committed app source matches except this progress note. No production C, JNI,
Kotlin or provider implementation changes. The explicit 18-hazard review is in
C_SAFETY_REVIEW.md; all initial failures and final evidence remain under
`.cache/android-wallet/resume-20260916/gcc-optimized-safety/`.
Hardware custody, physical-camera acceptance, parked TLS and prior root-lint
findings remain unresolved. The source audit of record/header publication,
recovery admission, JNI secret outputs and bounded RNG found no additional
product defect in the inspected paths. Next: inspect the managed recovery
record/entropy ownership boundary and its cancellation/error coverage.

2026-09-16: the real managed unlock worker now has explicit coverage for a
stored record changing after GCM decryption but before C promotion. Two new
instrumented cases cover pending and committed names. Each uses the existing
public software-GCM fixture and holds its actual provider callback after the
plaintext destination is filled. The test replaces only its own pre-existing
cache file with structurally valid, different ciphertext, releases the worker,
and requires OPERATION failure, no address publication, complete plaintext
erasure before UI dispatch, unchanged in-memory ciphertext and exact retention
of the replacement bytes/name. The ciphertext is never authenticated or accepted.

The fixture reuses the existing exclusive temporary-directory owner and worker
cleanup. All paths/names are fixed within that directory; the write refuses to
create a missing file. Callback entry, release, idle and termination waits stay
bounded. No platform key, wallet-v1 directory, real seed, authentication prompt
or global provider is used. No production source or C implementation changes.

All ten unlock tests pass without skips on API30/35/36 x86-64 emulators in
5.878/0.294/7.873 seconds. An isolated source-copy mutation removes only the
promotion-status check while retaining the actual promotion call. Both new
cases fail their expected OPERATION assertion; the live exact-pending and
closed-during-decryption controls still pass. The normal debug APK is then
restored on API35 and all four selected cases pass in 0.166 seconds. The
mutation never enters the checkout or release source, and its copied source is
restored after the pinned mutant APK and patch are preserved.

Android/JVM tests, both-ABI builds, debug/release lint, fixture isolation,
alignment, architecture and whitespace checks pass. Source-only tree
718cdf4e0f2edf0e57de0f8c84c793ef95027a59 reproduces the unchanged unsigned release
APK before the isolated debug mutation: 612,903 bytes, SHA256
`6c5d0c5f61ae9cd762f43df4fed9520ea05094dfd561c0b2f6715a8646f83f01`.
Committed app source matches except this progress note. Evidence and APKs are
under `.cache/android-wallet/resume-20260916/unlock-record-replacement/`.
This proves refusal for these observed storage changes, not protection against
an arbitrary later filesystem rollback or positive hardware custody. Existing
hardware, physical-camera, parked-TLS and root-lint limitations remain.
Next: inspect setup/authentication ownership across repeated foreground changes
and worker refusal, keeping hardware policy and live wallet state untouched.

2026-09-16: authentication callback acceptance is now isolated in a small
private success handler and exercised by eight public on-device regressions.
The platform callback forwards its nullable cipher to the same request-identity,
time-window and exact-cipher checks; foreground delivery remains in the existing
deliver method. No policy, timeout, key capability or acceptance condition changes.
The extraction adds no secret owner or new asynchronous operation. Cipher
comparison uses object identity, including when a replacement reuses the same
handle. Failure and duplicate callbacks cannot retire or approve another request.

Tests use an unattached Activity, uninitialized cipher handles and a receiver
that only counts prepared-action identities. They cover missing/different
ciphers, exactly-once delivery, background success/failure, cancellation, stale
callbacks after replacement, invalid clock origins and delivery-time rechecking.
Success calls the private handler; errors call the actual generated callback.
All state changes run on the main thread and each fixture cancels its own request.
No prompt, entropy, wallet session, file or platform key is opened. These tests
do not establish framework-result delivery, elapsed sleep behavior or hardware
authentication; the existing native deadline tests remain a separate claim.

The first prototype tried to instantiate a framework AuthenticationResult and
all eight cases stopped at API35 NoSuchMethodException. Its source and logs are
preserved. The constructor exists in the reviewed
[Android 15 framework source](https://android.googlesource.com/platform/frameworks/base/+/android-15.0.0_r1/core/java/android/hardware/biometrics/BiometricPrompt.java)
but is hidden from the public SDK. No hidden-API exemption, skip or platform
policy change was used. The final fixture tests the extracted production guard
with the nullable cipher value that the framework adapter forwards; every
behavioral assertion is retained.

All sixteen new/existing routing, setup-failure and native deadline tests pass
without skips on API30/35/36 x86-64 in 1.426/0.063/2.564 seconds. Three separate
isolated debug mutations remove exact-cipher rejection, foreground gating or
failure-request identity. They produce exactly 1/2/2 expected failures, each
with one passing control. After the normal APK is restored, all eight new tests
pass in 0.043 seconds. Mutated source copies are restored; their APKs, patches
and complete negative observations remain available.

Android/JVM tests, both-ABI builds, debug/release lint, APK fixture isolation,
alignment, architecture and whitespace checks pass. No C/provider source
changes; both packaged native libraries compare byte-for-byte with the previous
release. Source-only tree 7b9b26d31a790624ce990e53cd75506eee25e594 reproduces the
unsigned release APK on this host/toolchain: 612,903 bytes, SHA256
`64006c7ed890056749ad35b0e66a7cd056fd6edc915f94f84ffb74f766042bf2`.
Committed app source matches except this progress note. Evidence is under
`.cache/android-wallet/resume-20260916/authentication-callbacks/`.
Hardware custody, physical-camera acceptance, parked TLS and prior root-lint
findings remain unresolved. Next: inspect the UI-to-worker recovery input
handoff and early failure cleanup before ownership reaches the executor.

2026-09-16: six new Android recovery-submission regressions qualify the real
recovery keyboard/screen to platform-worker ownership handoff. Public invalid
marker text exercises ordinary and fatal callback failures, failure while
rendering the waiting screen, successful exact transfer, full queue refusal,
and queued cancellation for both confirmation and restoration. Assertions
observe both the original keyboard buffer and the transferred array; refusal
must erase before its deferred resource-failure callback. A held worker never
reaches wallet setup, entropy derivation, authentication, storage or keys.
Every worker gate/termination wait is bounded and owned temporary parents must
remain empty. No production Kotlin, JNI, C, provider or release behavior changes.

API35's accelerated emulator passes all six cases in 6.998 seconds; a fresh
isolated accelerated API36 emulator passes in 9.515 seconds. API30 passes two
three-case groups in 39.679 and 67.372 seconds. The original combined commands
on the three old unaccelerated profiles exceeded their 90-second outer limit.
API30's device log later reported all six passes, but that incomplete host run
is not substituted for the bounded group results. API35/36's old profiles remain
unqualified for this run; a stalled API36 logcat client was terminated. Their
profiles were not reset. The fresh API36 launch uses the qualified reaping
wrapper and local installed image; boot took 44.075 seconds. No emulator state
or build artifact enters Git.

An isolated source-copy mutation reverses the UI's transfer-cleanup guard.
Exactly five expected tests fail and the already-erased queue-refusal control
passes. The copied source is restored afterward; installing the normal debug
APK again yields six passes in 7.192 seconds. Mutation assertions include
embedded NUL diagnostics, so its preserved log must be read as text explicitly.
No assertion, deadline, permission, hardware policy or authentication check is
weakened in the accepted source.

Android/JVM checks, strict Kotlin compilation, debug/release lint, both-ABI
builds, fixture isolation, native alignment and whitespace checks pass. The
unsigned release hash remains
`64006c7ed890056749ad35b0e66a7cd056fd6edc915f94f84ffb74f766042bf2`.
Native analysis/sanitizer/fuzz evidence remains the preceding unchanged-C
checkpoint, not a new run. The unrelated flag-registry self-test remains open.
Evidence is under `.cache/android-wallet/mission-20260916/recovery-*`.
Next: explicit offline TLS security review, preserving its release quarantine.

## Android backup audit — 2026-09-18

The owner requested preservation before new development, exclusively on
`CesareFI/zclassic-android-wallet`, using the existing
`agent/android-jni-secret-retirement-20260915` branch. Initial checkout HEAD
`b0c3523bc3c48e24882d0f4403c2bfa8f1ebb7f5` was clean and already present on
`wallet-backup`. Both remotes were fetched. No main push, history rewrite,
branch deletion, worktree cleanup, or upstream integration is part of this
backup-only operation.

Audit of the other registered Android branches, dirty Android worktrees, and
stash found these preservation dispositions:

- The stash's Electrum framing harness and regression source are byte-identical
  to the versions retained by `29ea24238`; their CMake registration is present.
- The storage continuation is retained by `9eab9a0f9` and later storage and
  lifecycle commits. The JNI erasure candidate's full-width regression is
  retained by `afaea0a73`; later JNI retirement adds path/record erasure checks.
- Thirteen commits on the older security-hardening branch are absent by commit
  identity from the backup. Most have later implementations: caller-owned JNI
  output (`07b26ec0c` and successors), RNG checks (`e5e79f068`), backup failure
  cleanup (`b6d68e57c`), prepared HMAC (`24a688684`), BIP32/EC lifetime checks
  (`d200dfa09`, `ce8002b30`, `55a17c2ea`), preallocated session cleanup
  (`228e63b9b`), canvas restoration (`04388cf8b`), bounded camera allocation
  (`9c0e66717`), independent HMAC coverage (`19c5159e4`), and JNI storage guards
  (`afaea0a73`). Their original local branch remains untouched; these old commit
  identities are not claimed to have been pushed.
- Two remaining useful patches are recovered from that branch: queued recovery
  reference retirement from `341800596` with its JVM/Android tests, and early EC
  argument refusal from `fa14b2a38` with its allocation-observing regression.
  Existing current tests remain, including the later BIP32 retirement cases.
- The uncommitted header metadata contract is recovered from the header/security
  candidates, choosing the version with explanatory failure diagnostics. The
  uncommitted bounded Electrum span implementation, bytewise model, deterministic
  regression and fuzz target are recovered from the line-framing candidate.
  Only their CMake insertion points and the EC test insertion points needed
  adaptation to the current tree.
- The separate TLS candidate remains unvalidated and excluded under the existing
  parked review in TLS_REVIEW.md. Its provider patch and regression artifacts
  are left intact in its original worktree; no TLS reproduction or investigation
  was run and no TLS acceptance is implied.

All original worktrees, local branches and the stash remain intact. Only the
reviewed Android source, test, CMake and documentation paths are selected for
this preservation commit. Ignored caches, build trees, APKs, native binaries,
local SDK configuration, logs and benchmark outputs remain local. No operator
wallet, recovery seed, private key, credential or generated secret is included.
The configured generic commit identity is Wallet Development with an invalid
example-domain email. Exact remote equality is verified after publication,
without embedding a self-referential commit SHA in this entry.

Validation of the recovered snapshot: both Clang and GCC static analyzers and
production/test complexity caps pass; Clang ASan/UBSan passes 142/142 registered
tests, optimized GCC ASan/UBSan passes 137/137. Independent 31-second bounded
fuzz runs complete 28,370 framing and 433,907 header/record executions without
a finding. Offline wallet-core and Android JVM tests pass (including all ten
recovery-delivery cases), as do debug assembly, test-APK assembly, Android debug
lint, architecture-tree and whitespace checks. No device test was run in this
audit. Root lint-fast passes 31/32 gates; its unchanged flag-registry self-test
fails with an empty tracked-file scan. This is the already recorded unrelated
root-lint limitation, not a waived or weakened assertion. Raw logs remain in
ignored `.cache/backup-*` paths and are not committed.

### Preservation closeout and flag-registry investigation — 2026-09-18

Recovered source commit `d2d03143cc58822d9b49bf55448316e8b3325bea` was pushed
normally to the existing CesareFI development branch and verified using
`git ls-remote`. The follow-up audit adds evidence only, with no further
application, native, test or lint implementation changes.

Collected final validation, with zero failures in each listed test suite:

| Snapshot | Clang sanitizer tests | Optimized GCC sanitizer tests |
| --- | --- | --- |
| Pre-recovery `b0c3523bc` | 140 passed, 0 failed | 135 passed, 0 failed |
| Recovered `d2d03143c` | 142 passed, 0 failed | 137 passed, 0 failed |

The recovered Gradle reports contain 97/97 wallet-core tests and 50/50 Android
JVM tests, with zero failures, errors or skips. Debug and test-APK assembly,
Android debug lint, static analysis, complexity, architecture and whitespace
checks passed. Fuzz results remain 28,370 framing and 433,907 header/record
executions. No device or hardware-custody acceptance is asserted.

The flag-registry self-test failure has an observed environment cause. Its
unreadable-file fixture creates `fu_secret.c` with mode 000 and expects an
unreadable-file return. This session runs as UID 0 with permission-bypass
capabilities. System-call tracing observes `openat(..., O_RDONLY) = 4` despite
that mode, and the unmodified self-test exits 1. Running the same binary with
`setpriv --bounding-set=-dac_override,-dac_read_search` produces the expected
`EACCES` and self-test exit 0. The empty-scan diagnostic appears in both runs:
it belongs to an expected negative fixture and is not the failing assertion.
The binary SHA-256 is
`1f4d2e7205296eabeb02dd05d73c5b9f6bb60bb635eefe91df8943a486717127`.
Both gate source files and its shell wrapper have identical Git blob identities
at the pre-recovery baseline and recovered HEAD. No gate was edited, disabled,
suppressed or weakened.

After the self-test passes in the restricted environment, the real scan still
reports eight uses of five unregistered flags: `ZCL_HOST_CLANG`,
`ZCL_FIXTURE_TEST_VARIANT`, `ZCL_QUALIFY_TARGET`, `ZCL_QUALIFY_AAPT`, and
`ZCL_QUALIFY_FAULT`. An isolated Git fixture made from the baseline's exact
`flags.def` and the three reported Android tool scripts reproduces those eight
diagnostics byte-for-byte (`cmp` exit 0); those inputs are unchanged by the
recovery. Thus the self-test issue is environment-related and the real scan
issue is existing baseline registry debt. The fresh restricted-environment
`lint-fast` run remains 31 passed / 1 failed, not a global lint pass. Both
findings are documented without expanding this backup into tooling development.
Raw root/restricted traces, baseline diagnostics and lint output remain in
ignored `.cache/backup-flag-*` and `.cache/backup-lint-fast-bounded.log`.

Final inventory: A means already represented or superseded in current HEAD;
B means recovered in `d2d03143c`; C means incomplete/quarantined. All listed
older worktrees and the stash remain intact; none was deleted or cleaned.
The current `/root/z23-android` checkout is the sole publication checkout.

| Remaining worktree or stash | Disposition and comparison |
| --- | --- |
| `/tmp/z23-android-header-contract-20260914` | B: regression is byte-identical; current fuzz contract adds only explanatory diagnostics to the old contract; registration retained. |
| `/tmp/z23-android-line-framing-20260915` | B: implementation, bytewise fuzz model and regression are byte-identical; registrations adapted to current CMake. |
| `/tmp/z23-android-security-hardening-20260914` | A/B: current header files, EC preflight and recovery-delivery implementation match the candidates exactly; other useful changes have the successors listed in the preceding audit. Its 13 old commit identities remain local, with their useful behavior represented in current history. |
| `/tmp/z23-android-jni-erasure-20260914` | A: full-width entropy regression retained; current fixtures additionally check path/record/publication retirement and portable JNI types. The old fuzz helper split adds no missing behavior. |
| `/tmp/z23-android-wallet-storage-20260913` | A: paired persistence, restore routing and tests retained; current code additionally rejects orphan state and clears JNI scratch. WalletStorage and WalletPersistenceTest match exactly. |
| `/tmp/z23-android-tls-empty-name-review` | C: unfinished provider patch and TLS test artifacts left untouched; no TLS test, reproducer, build or investigation run. |
| `stash@{0}`: inherited Electrum framing tests | A: fuzz_electrum.c and test_electrum_fuzz.c have exactly matching Git blob identities in the stash and HEAD; frame warning and regression registration are present. |

Every registered Android branch other than the older security-hardening branch
has zero commits absent from the fetched CesareFI remote. No additional useful
missing change was found in the final review. Generated artifacts, SDK-local
configuration, caches, logs and benchmark outputs remain excluded. TLS quarantine
and all production/custody boundaries are unchanged. Exact remote SHA equality
must be checked again after publishing this documentation-only closeout.

## Production continuation: queued cleanup failure — 2026-09-18

The owner resumed permitted production engineering while retaining the TLS
quarantine, platform restrictions, custody gates and original Zclassic rules.
The next concrete risk was executor shutdown after a queued cleanup throws:
`OwnedExecutor.close` marked the owner closed, then abandoned its drain before
installing the session finalizer or calling shutdown. Later queued inputs and
the process admission could remain retained. Cleanup callbacks still must be
bounded and nonthrowing; the owner now contains a broken callback's effect on
other independently owned inputs.

A new JVM regression failed on the unchanged source after observing only one
queued cleanup instead of four. The minimal fix attempts every queued cleanup,
installs the existing session finalizer, attempts shutdown, and then rethrows
the first failure without allocating suppressed-exception storage. The active
operation remains responsible for its own finally block, and session retirement
still waits for termination. No worker, retry, extra queue slot, or platform
restriction bypass is added.

Tests inject ordinary/fatal cleanup failures plus a second cleanup failure,
check all public marker arrays are cleared, no cancelled action runs, the queue
is empty, shutdown is admitted, the active worker is not finalized early, cleanup
runs once, and both process admissions become available afterward. The full
executor JVM group passes 8/8. The actual ART executor suite passes 3/3 with zero
skips on API35 (0.066s) and API36 (0.042s). Fixtures contain only public characters
and owned workers, with bounded cleanup even against the original broken code.

Offline wallet-core/Android JVM tests, strict Kotlin compilation, debug/test
and both-ABI release builds, debug/release lint, fixture isolation, 16 KiB
alignment, architecture and whitespace checks pass. Native source and build
configuration are unchanged; the previous 142/142 Clang and 137/137 optimized
GCC sanitizer/static-analysis evidence remains the native baseline, not a new
run. Physical-device custody and the documented baseline root-lint findings
remain open. Local evidence is in ignored `.cache/executor-drain-*` paths.
Next: authentication failure notification when prompt cancellation itself throws.

## Production continuation: authentication cancellation notification — 2026-09-18

Failure retirement cleared the pending authentication request before cancelling
its platform signal. A throwing cancellation listener then prevented foreground
failure notification or background deferral, leaving the UI wait unresolved.
Two new callback regressions failed on unchanged source: 8/10 passed, with
foreground delivery absent and background deferral absent. Cancellation now
attempts failure notification even when the listener throws, then rethrows the
first error without allocating suppressed-exception storage. Request identity,
timeout removal, foreground-only delivery, cipher identity and authentication
policy are unchanged.

The tests cover ordinary and fatal synthetic cancellation errors, a secondary
notification error, exactly-once foreground/background failure, and inert late
callbacks. They use public CancellationSignal listeners and uninitialized
cipher handles in the existing owned callback fixture. No prompt, key, wallet,
storage operation or framework security modification is involved; this is
callback-routing evidence, not hardware-authentication qualification.

API35 and API36 each pass all 16 callback/setup tests with zero skips (0.085s
and 0.079s). Offline wallet-core tests pass 97/97 and Android JVM tests 51/51.
Strict compilation, debug/test and both-ABI release builds, debug/release lint,
fixture isolation, 16 KiB alignment, architecture and whitespace checks pass.
No native source or build configuration changed; the previous Clang/GCC native
safety and static-analysis baseline remains applicable without a new run.
Ignored `.cache/auth-cancel-*` logs retain local evidence. The previously
documented root-lint findings and hardware custody gate remain open, and TLS
quarantine is unchanged. Next: establish a bounded public-fixture measurement
of the read-only sync path before proposing any performance change.

## Production continuation: bounded read-only sync measurement — 2026-09-18

The sync path had correctness fixtures but no reusable timing tool for complete
framed request/reply processing. `bench_read_only_sync` now reuses pinned public
genesis and zero-hash-address fixtures for both networks, balance-only and
two-entry history, and 1/256/4096-byte fragments. Every attempt verifies the
complete public snapshot, including exact balance, history IDs/heights,
address/source, freshness and report availability. Fixture generation and the
single checked 47,488-byte workspace allocation are outside measured batches.
No socket, wallet, key, storage, TLS or Android lifecycle enters the tool.

An x86_64 AMD EPYC 7402P host measured these median thread-CPU microseconds per
complete attempt, each from five batches of 100 after ten warm-up attempts.
Release profiles use Clang 20.1.2 or GCC 14.2.0 and retain existing provider
optimization flags; sanitizer timings are kept separate.

| Network/profile | Fragment bytes | Clang CPU µs | GCC CPU µs |
| --- | ---: | ---: | ---: |
| Mainnet balance | 1 | 182.715 | 164.235 |
| Mainnet balance | 256 | 138.558 | 119.994 |
| Mainnet balance | 4096 | 138.576 | 119.678 |
| Mainnet history | 1 | 191.701 | 174.056 |
| Mainnet history | 256 | 146.862 | 128.691 |
| Mainnet history | 4096 | 146.574 | 128.353 |
| Testnet balance | 1 | 187.080 | 165.543 |
| Testnet balance | 256 | 140.007 | 121.262 |
| Testnet balance | 4096 | 139.674 | 120.767 |
| Testnet history | 1 | 198.451 | 175.663 |
| Testnet history | 256 | 153.834 | 130.099 |
| Testnet history | 4096 | 153.172 | 129.711 |

This is a local CPU baseline including verification overhead, not live sync,
device, battery or consensus evidence. It does not justify changing any
validation or cleanup contract. The measured paths and production code are
unchanged. Raw timing/build evidence remains in ignored `.cache/sync-benchmark-*`
files; reproducible commands and limits are in `READ_ONLY_SYNC.md`.

Manual C safety review: all large buffers share one checked fixed-size allocation
with one cleanup/free path; no multiplication-based allocation, retained pointer,
recursion, VLA or mutable global is introduced. Array indices are bounded by
six/seven replies; fragment arithmetic subtracts only after offset bounds and
advances only by the checked remaining span. Size-to-ID conversions are bounded
by seven. The simulated clock checks overflow before increment. Clock samples
are used only after successful reads. Request lengths are checked before tail
access, framing lengths before comparison, and history count before indexing.
Format arguments match their types. All error paths return through owned
cleanup, with no double free, use-after-free, dangling pointer, uninitialized
read or unchecked allocation. There is one thread, bounded work and bounded
stack use; fixtures contain only public data. Existing parsers continue to own
malformed-input refusal. No production serialization or consensus change occurs.

Clang native safety passes 142/142 (132.02s), optimized GCC 137/137 (190.30s),
both with zero failures. Each sanitizer benchmark also completes all 12 profiles
and 60 timed samples. Clang analysis and GCC `-fanalyzer` pass for the new tool;
the full enabled-core/provider analysis, pinned digests and complexity checks
pass. Android/JVM tests, debug/test and both-ABI release builds, both lint
variants, fixture isolation, 16 KiB alignment, architecture and diff checks pass.
The documented root-lint baseline and physical custody gate remain open; TLS
review stays OFF. No generated benchmark output or build artifact is committed.
Next: register the five existing Android build/test environment flags responsible
for the eight previously confirmed catalog diagnostics.

## Production continuation: Android build/test flag catalog — 2026-09-18

The real flag-registry scan reproduced eight diagnostics for five existing
Android environment variables. Added their declarations to the existing
`engine/composition/flags.def` catalog with exact read sites, defaults and
purposes: the host compiler selector, fixture variant, and three qualification
metadata-test variables. Once registration passed, first-use validation exposed
the existing unzip entry's stale line53 reference; its actual read is line175.
Only catalog metadata changed. No script, checker, fixture, security restriction,
APK policy or runtime behavior was modified.

The actual catalog gate now passes: 1226 registered flags, 2138 read sites,
zero unregistered/expired entries and 1207 verified first-use pointers. Normal
fixture isolation, debug/release lint and 16 KiB alignment pass; the separate
qualification APK build passes its unchanged control and all 20 negative
metadata cases. Architecture and whitespace checks pass. The prior native
142/142 Clang and 137/137 optimized GCC evidence applies unchanged.

The standalone flag-registry selftest still fails under this root environment;
`make lint-fast` remains 31/32. The earlier mode000/capability diagnosis remains
applicable: the unreadable-file case expects refusal while this process can
read the file. The selftest constructs its own catalog, so these real-catalog
declarations do not affect it. No assertion was changed, skipped or disabled,
and no privilege or platform-policy change was used in this continuation.
Evidence is in ignored `.cache/android-flags-*` logs. TLS quarantine remains
untouched. Next: verify unsigned APK reproduction from a clean source-only
copy containing the new executor/authentication fixes.

## Production continuation: current release reproduction and milestone accuracy — 2026-09-18

A source-only archive of commit
`67ed27f9b39faea59d690a63d099988a6f83800d` supplied the app, pinned Android
providers and its two C package dependencies to a fresh nested source directory.
No project build, `.cxx` or `.gradle` output was copied. The only local input
added was the SDK path. Offline release assembly with `--no-build-cache`
executed all 57 tasks successfully in 30s. Complete APK comparison with `cmp`
passed against the active checkout's unsigned release, including both ABIs.
Each file is 641,227 bytes with SHA256
`147e717f0c470c609375ff185cd95f3b786be5f5ddc8322ba5cfe59cfdac30f8`.

Toolchain: Gradle8.13, AGP8.13.2, Kotlin2.2.21, OpenJDK17.0.20,
Android build-tools35.0.0, compile SDK36 and NDK27.2.12479018 on Linux x86_64.
The selected source archive has SHA256
`3c7bcdf668441d585a3f6efb8e6bb6926f22cf840df472a28969bf2b022655cc`.
Logs, archive, SDK-local configuration and APKs remain ignored under
`.cache/reproduce-67ed27f9b/`. This establishes unsigned byte reproduction across
two paths on one host/toolchain; independent-host, signed-release and physical
custody qualification remain open.

Review also found that `NEXT_MILESTONE.md` and `TRANSACTIONS.md` still described
the already preserved mixed-source assessment/commitment work as future scope.
Their descriptions now match the explicit internal APIs and existing regression
coverage. Older narrow entry points, JNI/review/signing admission, source-trust
limits and the commitment preimage guard remain explicit. No implementation or
test changed in this documentation slice. The current native/Android validation
remains applicable; the root-lint environment limitation remains documented.
Next: inspect camera and foreground presentation cleanup for a concrete lifecycle
or callback-ownership gap, keeping TLS and platform-security restrictions intact.

## Production continuation: scanner connection setup retirement — 2026-09-18

`ScanDecodeClient.connect` handled ordinary setup exceptions but let fatal
errors escape without retiring its lifetime. A timeout post that enqueued before
throwing retained the callback and left the failed client open. Three new
regressions failed on unchanged source; two ordinary-exception/refusal controls
passed. The minimal fix closes setup before propagating a fatal error and
preserves that original error even if failure notification also throws.

The five-test fixture uses public Handler and ContextWrapper overrides, with
reflection confined to this application's owned fields. It checks failures
before/after enqueue, timeout removal, no service binding, no revived retry,
inert stale wakeups and exactly-once notification. It creates no service,
camera, wallet or key, changes no platform policy and causes no actual memory
exhaustion. Existing actual isolated-service readiness tests supply the separate
real Binder control.

The combined new connection, existing submission and actual readiness suites
pass 14/14 with no skips on API30/35/36 in 5.054/0.391/0.481s. The preceding
executor/authentication changes also pass all 19 instrumented tests on API30
(1.999s), closing their oldest-supported-API coverage gap. Offline JVM tests,
debug/test and both-ABI release builds, debug/release lint, fixture isolation,
16 KiB alignment, architecture and diff checks pass. Native source is unchanged;
both release JNI libraries compare byte-for-byte with the previous reproduced
APK, retaining the recent 142/142 Clang and 137/137 optimized GCC safety evidence.

Fresh source-only tree `e9430d31ac1c02a97385a1167fbdb25463df7d48`, containing the
exact staged implementation/tests and preceding documentation, reproduced the
complete 641,227-byte unsigned release APK with all 57 tasks executed and the
build cache disabled (19s). Both paths have SHA256
`3dd15715169e8cb34c364a025656707838a523283d018761ac71388393100cb0`.
Toolchain and reproduction limits are unchanged from the preceding checkpoint.
Evidence remains in ignored `.cache/scan-connect-*`,
`.cache/continuation-api30-executor-auth.log` and `.cache/reproduce-scan-connect/`.
Root-lint selftest, physical custody/camera and independent-host release limits
remain open; TLS quarantine is unchanged. Continue with bounded foreground
delivery and shutdown ownership review rather than resuming quarantined work.

## Production continuation: submission error identity — 2026-09-18

The executor's submission cleanup could replace a worker-start error with a
second cleanup exception. A new JVM regression reproduced this, receiving an
IllegalStateException instead of the original synthetic OutOfMemoryError.
Allocation/startup failure paths now attempt owned cleanup and rethrow the first
failure without allocating suppressed-exception storage. Ordinary queue
rejection still returns false after successful cleanup; its cleanup failures
remain visible. The task-allocation catch follows the same first-error rule.

New JVM and ART cases exercise both pre-enqueue and enqueue-before-worker-start
failure, public-array clearing, exactly one cleanup, an empty failed queue,
successful independent retry and release of both process admissions. Existing
assertions are retained. JVM executor tests pass 9/9; the full offline JVM groups
pass 97 wallet-core and 52 Android tests. The executor instrumentation passes
4/4 without skips on API30/35/36 (0.685/0.052/0.038s). Debug/test and both-ABI
release builds, both Android lint variants, fixture isolation, 16 KiB alignment,
architecture and whitespace checks pass. No native source changed; both packaged
JNI libraries remain byte-identical to the preceding reproduced release, so the
recent Clang142/GCC137 safety and analyzer evidence remains applicable.

Fresh source-only tree `f2e09789773b8e35085b077310dc03bc6f39c154` reproduces the
641,227-byte unsigned APK with every release task executed and build caching
disabled (18s). Complete `cmp` passes; both APKs have SHA256
`0eb29e8352139bce06b25fd46b9f10013c99acca14653ef92c34075569b8eaae`.
The source tree contains the exact implementation/tests and prior documentation;
this progress note is the only subsequent change. Public synthetic failures do
not exhaust memory or alter platform restrictions. Logs/artifacts remain ignored
under `.cache/submit-primary-*` and `.cache/reproduce-submit-primary/`.
TLS quarantine and existing physical-device/root-lint limits remain unchanged.
Next: inspect read-only JNI snapshot allocation and measurement opportunities,
keeping normal validation authoritative and avoiding speculative optimization.

## Production continuation: retired sync replies avoid frame ownership — 2026-10-03

Resumed the six-file pending Android sync continuation on `b4d5d6a8f`, after
fetching upstream and reconciling the intervening committed preservation work.
The retained RED fixture counted 64 native frame allocations and 64 Java byte
region reads for 64 replies from an already retired attempt. C already rejected
these tokens without changing the current attempt, but JNI copied their frames
first. The existing C token predicate is now a documented read-only adapter
preflight, called under the same registry lock before frame allocation/copy.
Current replies retain all prior parsing, time, deadline and retirement checks.

GREEN checks zero allocations/copies for unissued, cancelled and superseded
tokens in balance/history modes, unchanged snapshots despite a future callback
time, and normal allocation/copy plus progress for an admitted current reply.
This is an exact operation-count improvement; no latency or battery claim is
made. Existing exception and allocation-failure checks remain intact.

Retained completed validation for these unchanged sources passes Clang142/GCC137
native groups with ASan/UBSan/LSan, provider digests, both analyzers and complexity
caps; the JNI sync fuzzer completes3514 runs in21s without a finding. Offline
Android/JVM, debug/test, both-ABI minified release, both lint variants, fixture
isolation and16KiB alignment pass (154 tasks). A fresh fully TSan-instrumented
core/provider/JNI race fixture passes. SHA256 records the six source/test files.

Fresh public-fixture instrumentation passes all three read-only sync tests with
no skips on API30/35/36 x86_64 (0.041/0.074/0.048s). API35 reports a16384-byte
page size. The initial reused API35 profile was credential-locked after boot,
and Android refused to launch the test process; its state and policy were left
intact. A new isolated public-fixture AVD supplied the successful API35 run.
This establishes emulator/JNI behavior, not physical ARM64 or hardware custody.
Architecture, documentation-count and whitespace gates pass. Evidence remains
ignored under `.cache/sync-retired-*`; no generated artifact is committed.

The explicit C hazard review is in `C_SAFETY_REVIEW.md`. TLS remains quarantined;
no consensus, recovery format, wallet state or production data changed. Next:
strengthen concurrent late-reply coverage against replacement attempts under
TSan, then continue independent native storage/restart review. Worldstream and
production operations remain outside this mission.

## Production continuation: concurrent retired replies — 2026-10-03

Extended the existing JNI registry race fixture with two reply threads while a
replacement attempt remains active, followed by repeated attempt replacement
and owner closure. Across 16 rounds, 10,240 old-token replies must return
CANCELLED without reading their thread-owned public byte arrays. Current
requests and snapshots retain their exact admission/state assertions. A barrier
holds the first replacement active throughout the first reply phase, making
wrong-sequence admission detectable independently of scheduling.

The unchanged control passes. Removing JNI preflight fails the zero-read
assertion; removing the C sequence match fails cancellation/state assertions.
An initial test setup incorrectly expected the previous cancellation to disappear
on begin. The documented behavior retains it while refreshing; the fixture was
corrected, leaving existing assertions and production code unchanged. Its
initial failure is retained separately and is not counted as product RED evidence.

Focused Clang ASan/UBSan/LSan, optimized GCC sanitizer and fully instrumented
TSan runs pass. Clang and GCC static analyzers and the unchanged test complexity
cap pass (no function above15). Strict NDK release-archive fixtures pass on
API30/35/36 x86_64, including API35 with16384-byte pages; owned temporary device
paths are removed. ARM64 compiles with16KiB alignment only. The preceding APK,
JVM, lint and full native-core evidence remains applicable because this slice
changes only the host/native fixture. Evidence remains in `.cache/sync-race*`.
Next: continue the independent native storage/restart and resource review;
physical custody and TLS quarantine remain separate unchanged gates.

## Production continuation: seal entropy retires before public readback — 2026-10-03

The seal path retained entropy while committing and rereading the public record,
then derived the receive address afterward. A new actual-worker/ART fixture
observed nonzero restored entropy at the post-encryption storage-admission clock
on unchanged source. Its one-shot observation cannot be overwritten by a later
UI-clock read. The fixture uses only a nonzero public 12-word entropy vector,
a fixed public software cipher and an exclusively created temporary directory.
It does not qualify or bypass production Keystore authentication.

The thin platform owner now finishes C receive-address derivation before storage.
Restored entropy is cleared before admission because restoration persists only
ciphertext and must not initialize change history. Fresh creation retains entropy
through C's required initial authenticated change-state commit, then clears it
and setup before the public record-verification read. Finally cleanup still
covers encryption, derivation, admission, persistence and readback failures.
Derivation failure now precedes any persistence. Native cryptography, recovery
outputs, record bytes, storage durability and authentication policy are unchanged.

RED fails the explicit plaintext-lifetime assertion; GREEN passes. The combined
setup-seal, session-close and unlock-output suites pass 29/29 without skips on
API30/35/36 x86_64 (1.015/0.785/0.818s), including API35 with16KiB pages. Existing
controls cover creation and restoration, GCM failures, expired admission,
background close during provider work, delayed delivery and exact persisted
record/address behavior. Offline JVM tests, debug/test, both-ABI minified release,
debug/release lint, fixture isolation and16KiB alignment pass (154 tasks).
Native implementation is unchanged; the preceding native safety evidence applies.
Architecture, documentation-count and whitespace gates pass.

A fresh source-only archive of `6b893dd81` plus the two exact changed files,
with no project caches/build outputs copied and build caching disabled, executes
all57 release tasks in25s. Complete unsigned APK `cmp` passes across the two
paths; SHA256 is
`2cf6ac1c15e3d0c1068fe9387dca2617bb07d7ac702bde9444ae3859c3bb971d`.
This is same-host/toolchain unsigned reproduction, not independent-host,
signed-release or physical-device custody evidence. Raw proof remains ignored
under `.cache/seal-retirement-*` and `.cache/reproduce-seal-retirement/`.
Next: continue bounded native storage/startup failure review and measured
resource work. No Worldstream, consensus, production data or TLS change occurred.

## Production continuation: storage fixture acquisition cleanup — 2026-10-03

Review before storage measurements found that the shared native fixture helper
leaked its newly created temporary directory when descriptor open failed. The
new regression injects EMFILE without exhausting real descriptors. The old
helper fails the directory-retirement assertion; the corrected helper removes
its own fresh empty directory and preserves the original error. A secondary
rmdir refusal is reported, keeps that directory available for diagnosis and
still preserves EMFILE. The test cleans its invocation-owned RED/refused residue
and verifies a separate active fixture is unaffected.

Initial Clang RED was valid, but optimized GCC bypassed macro interposition via
its fortified open alias. The final regression uses existing linker-wrap practice
and passes both compilers without weakening libc protection. The final fixture
also fails against the unchanged old helper and passes against the fixed one.
The C hazard review covers the helper, wrappers and ownership boundaries.

All 28 registered storage/change groups sharing the helper pass with Clang and
optimized GCC ASan/UBSan/LSan (5.52/18.21s). Both deadline-contract gates, static
analyzers and test complexity cap pass; no function exceeds15. The real fuzz
profile/manifest mutation gate passes in54.82s with unchanged deadlines. Storage
fuzzing completes33,664 executions in21s without a finding. Release-archive
regressions pass on API30/35/36 x86_64, including API35 with16384-byte pages;
ARM64 compiles/alignment only. Architecture/docs/diff gates pass. This test-only
change leaves the validated application, cryptography, custody and APK unchanged.

Evidence remains ignored under `.cache/storage-fixture*`; an initial attempted
fuzz build pointed at a corpus directory, was diagnosed and replaced with a
separate configured build. No generated artifacts or temporary directories enter
Git. Next: establish a bounded, verified native storage/startup timing baseline
before considering any write-path optimization. Durability checks remain intact.

## Production continuation: verified native storage baseline — 2026-10-03

Added the explicit, default-build-excluded `bench_wallet_storage` host tool.
It reuses the public 12-word fixture and isolated storage helper, verifies every
read/status/record/pending flag, and retains normal locks, validation and fsync.
No application storage or cryptographic implementation changed. The tool covers
empty/pending/committed startup reads, committed promotion plus readback, and
fresh wallet-only creation plus readback. Documentation and exact commands are
in `WALLET_RECORD.md`; it accepts no caller-selected path.

Five samples follow one warm-up batch per profile. Reads/promotion use128 calls
across eight stores; creation uses eight fresh stores once each. Preparation,
public-vector derivation and cleanup are untimed. The following medians are
microseconds per verified operation on Linux x86_64, AMD EPYC7402P, `/tmp` on
ext4, Clang20.1.2/GCC14.2.0 Release profiles. Runs were serialized; no CPU/cache
isolation or physical-device performance claim is made.

| Profile | Clang wall µs | Clang CPU µs | GCC wall µs | GCC CPU µs |
| --- | ---: | ---: | ---: | ---: |
| Empty read | 277.684 | 58.385 | 284.124 | 61.367 |
| Pending read | 291.584 | 74.035 | 290.101 | 65.568 |
| Committed read | 268.446 | 66.248 | 282.829 | 69.572 |
| Promotion + read | 724.870 | 90.273 | 791.440 | 157.715 |
| Creation + read | 1661.970 | 239.889 | 1881.239 | 402.619 |

These observations establish a baseline only; compiler differences and wall/CPU
gaps are not isolated optimization effects. No durability or validation removal
follows from them. Three deliberate read-result mutations (record byte, pending
flag, failed-output length) are rejected. A separate test-only acquisition audit
verifies retirement of all240 control directories and all56/56/8 directories from
the failed cases. The benchmark's own complete cleanup path runs on each refusal.

Clang/GCC ASan/UBSan/LSan executions complete all25 reported samples, and both
static analyzers and test complexity caps pass. The actual fuzz-profile/manifest
mutation gate passes in55.63s with unchanged deadlines. Strict NDK builds cover
both ABIs; all profiles execute successfully on API30/35/36 x86_64, including
16KiB API35. Those are runtime correctness observations, not physical flash or
ARM64 timing evidence. Minified release, alignment and fixture-isolation gates
pass; complete APK comparison with the preceding reproduced release passes.
Architecture/docs/diff checks pass. Raw proof remains in `.cache/storage-benchmark*`.

Next: continue native/platform failure and lifetime review using these measured
limits; keep storage durability and correctness authoritative. No production
node/data, real wallet, consensus predicate, TLS scope or Worldstream work changed.

## Production continuation: fuzz large sync-clock transitions — 2026-10-03

The read-only watch fuzzer previously advanced at most128 byte-sized steps, so
its main state machine could not naturally explore the60-second freshness span
or uint64 clock boundaries. It now preserves ordinary short steps and adds
freshness-sized advances, saturation, near/max clocks and an arbitrary eight-byte
clock. Independent invariants require a successful deadline to be later than
now and report age not to exceed elapsed time since clock zero. Five deterministic
replays cover complete balance/history reports, expiry, rollback and large clocks.

The new strict replay target exposed optimized GCC test frames of5200/4736 bytes.
Large public fixtures now use explicitly reset, bounded storage within the
single-threaded harness; the4096-byte gate remains unchanged. This is test storage
only, with no production global or lifetime change. Deliberate deadline-overflow
and rollback-check mutations fail with both Clang and GCC; unchanged controls
pass. An initial overflow mutation did not compile due to an unused parameter;
it was corrected without disabling a warning and is not counted as RED evidence.

The final seeded ASan/UBSan fuzz campaign completes18,028 executions in31s without
a finding. Clang and optimized GCC each pass the watch/replay and two deadline
contract groups (4/4); static analyzers and complexity caps pass. The real fuzz
profile/manifest mutation gate passes in55.45s. Strict release-archive replays
pass API30/35/36 x86_64, including16KiB API35; ARM64 compiles/alignment only.
Architecture/docs/diff gates pass. Application source, cryptography, APK behavior
and the preceding JVM/instrumentation qualification remain unchanged. Evidence:
`.cache/sync-clock*` (ignored).

Next: continue safe native malformed-input/resource-failure and Android lifetime
review. The measured storage baseline does not authorize removing fsync or
validation. Physical custody and TLS review remain open, separate boundaries.

## Production continuation: complete-only JNI sync errors — 2026-10-03

An active-attempt regression reproduced partial error publication: history
projection wrote refreshing/deadline metadata before rejecting an invalid history
count or availability flag. Kotlin already rejects the status; no erroneous UI
balance or secret disclosure is claimed. Both native snapshot entry points now
clear payload on any error after registry unlock, with fixed10/12-word error
packets. Successful packets, owner lifetime and authoritative clock updates stay
unchanged. A common fake-VM assertion checks every returned error packet; the
regression also proves the active attempt retains its original deadline.

RED on unchanged production fails the zero-payload assertion; GREEN passes.
Deliberately removing clearing fails Clang and GCC controls independently.
Full C safety passes144 Clang and139 optimized GCC groups with ASan/UBSan/LSan,
provider hashes, static analysis and complexity caps. The final portable fixture
also passes both focused host groups and analyzers. TSan sync races pass.
The first fuzz run completed only3 inputs because coverage-function symbolization
took90s; a bounded run disabling only that progress output completes7,805 inputs
in31s without a finding. No validation predicate or timeout was weakened.

NDK initially rejected the OpenJDK-only fixture table tag. The existing adjacent
Android/OpenJDK alias pattern fixes the test portability boundary. Native fault
regressions and all3 actual ART sync tests pass each ofAPI30/35/36 x86_64,
including16KiB API35. ARM64 compiles/alignment only. Android JVM/debug/test and
both-ABI minified release, lint, fixture isolation and alignment pass154 tasks.
Architecture/docs/diff gates pass. Evidence: `.cache/snapshot-error*` (ignored).

A fresh source-only build at351fe3a88 plus the two exact changed C files,
without project/build caches, executes all57 release tasks in21s. Full unsigned
APK bytes match SHA256
`84e6edf572e0aaf9f0ff90118406dbc864afbc59b6a968fbff4cdd57d3311fee`.
Reproduction inputs/hashes: `.cache/reproduce-snapshot-error/` (ignored).

Next: extend failure qualification to uncertainty after successful projection,
including full history packets; retain status-only errors, fixed packet lengths
and complete native retirement. Hardware custody and TLS remain separate gates.

## Production continuation: late snapshot failure qualification — 2026-10-03

A test-only mutex adapter now releases the real fixture lock, then can report
one synthetic uncertain outcome. This exercises refusal after successful native
projection without leaving a locked test resource or changing production locks.
With a complete16-entry report, both snapshot profiles must publish status-only
errors, and the following history packet must match all156 original words.
Native snapshot/output retirement assertions remain active.

Removing balance payload clearing or history error-length reset independently
fails the new regression under Clang and GCC; unchanged controls pass. Focused
strict ASan/UBSan/LSan builds, both static analyzers and the test complexity cap
pass. The JNI fuzzer now combines projection corruption, unlock uncertainty and
both packet profiles; it completes6,793 executions in31s without a finding.
The real fuzz-profile manifest mutation gate passes55.61s and its deadline
contract passes1.20s. Native fixtures pass API30/35/36 x86_64 including16KiB;
ARM64 compiles only. Production source and APK bytes are unchanged, so preceding
ART/release/reproduction evidence remains applicable. No new concurrency claim:
TSan covers the separate unchanged production race target from the prior slice.
Evidence: `.cache/snapshot-unlock*` (ignored).

Next: continue custody/platform failure and restart review. Preserve the tested
JNI error boundary, exact12-word recovery, TLS quarantine and physical-device
custody gate; no production wallet/data or node activity is authorized.

## Production continuation: retire owned address seed before public conversion — 2026-10-03

The entropy-based receive/change wrapper retained its local64-byte seed while
final public-key conversion and address hashing ran. A live-lifetime regression
reproduces this on unchanged production. The wrapper now derives the final key,
clears the seed, then uses a shared bounded public conversion/encoding helper.
The existing private scalar/chain-code wipe still precedes public hashes.
Borrowed seed/context APIs, BIP39/BIP32 paths and output/failure semantics stay
unchanged; this is custody hardening with no performance claim.

Both external and internal address profiles pass success plus injected master,
final public-key and public-hash failures. Moving the seed wipe back after
encoding fails independently under Clang and GCC, while controls pass. Full
native safety passes144 Clang and139 optimized GCC groups with ASan/UBSan/LSan,
independent address oracle, provider hashes, static analyzers, complexity caps
and unchanged deadline/manifest gates. A first fuzz command selected an oracle-
only target absent from that build; the applicable JNI-key target instead
completes15,924 executions in31s without a finding. No gate was weakened.

The native secret-failure fixture and32 actual ART key/setup/unlock/close tests
pass each ofAPI30/35/36 x86_64, including16KiB API35. ARM64 compiles/alignment
only. Android JVM/debug/test/both-ABI minified release/lints/fixture isolation and
alignment pass154 tasks. Architecture/docs/diff checks pass. Evidence:
`.cache/receive-seed*` (ignored).

A fresh source-only build atffe0a7b6b plus the three exact changed native files,
without project/build caches, executes all57 release tasks in19s. Full APK bytes
match SHA256 `19c50e6b987f74c0f06052d83764d931537ebe1b7dd5faec0b68a334541e853f`.
Inputs/hashes are in `.cache/reproduce-receive-seed/` (ignored).

Next: continue secret-provider ownership and platform restart/failure review;
preserve completed address vectors, output canaries and device evidence. Physical
hardware custody, chain/unspentness admission and TLS security review remain
explicit separate boundaries. No real wallet, production data or consensus
behavior changed.

## Production continuation: interrupt recovery promotion and durable retries — 2026-10-03

The storage crash fixture now interrupts pending-record promotion after file
flush, pending-directory flush and rename, plus committed-record retries after
file and directory flush. Every resumed owner must observe the exact record and
expected pending state, finish promotion, and continue refusing overwrite. Output
tails retain canaries. Five creation interruptions and twelve competing creators
remain active; all fixtures contain only public vectors and inert ciphertext.

A missing recovery-file fsync mutation passes the previous crash executable and
fails the expanded executable under both Clang and GCC. Controls pass with strict
ASan/UBSan/LSan, both static analyzers and the test complexity cap. The initial
analyzer command incorrectly included a linker-only flag; flags were separated
without suppressing its warning. API30/35/36 initially exposed that Bionic's
fortified __write_chk bypassed the write hook. The fixture now interposes both
entry points and forwards original invalid bounds to the real fortified call.
No production fortification was disabled. Final release-archive crash fixtures
pass all three x86_64 APIs, including 16KiB API35; ARM64 builds/alignment only.
Host CTest remains within its unchanged 30-second deadline. Architecture/docs/diff
checks pass. Evidence: `.cache/storage-promotion-crash*` (ignored).

No product source, APK, parser or cryptographic code changed; preceding fuzz,
ART and release reproduction evidence remains applicable. Process termination
cannot prove power-loss persistence, and promotion here does not authenticate
GCM. Next: continue safe storage/restart failure coverage and custody/native
ownership review, with physical custody, authenticated chain state and TLS
quarantine preserved.

## Production continuation: exercise fortified Android storage failures — 2026-10-03

The shared storage fault injector handled fortified reads only under glibc and
had no Bionic fortified-write adapter. The unchanged fixture against the Android
release archive reproduced the gap: a requested one-byte write sequence was
bypassed and its exact call-count assertion failed. The test helper now covers
Bionic __read_chk/__pread_chk/__write_chk, retaining original capacity refusal
and real fortified forwarding. Product source and release artifacts are unchanged.

Nine dependent storage/change fault/retirement groups pass strict Clang and GCC
ASan/UBSan/LSan (1.67s/2.72s). Both host analyzers, Android ARM64-target analyzer,
complexity and architecture/docs/diff gates pass. Three actual release-archive
executables (wallet storage faults, change storage faults and recovery probe
faults) pass API30/35/36 x86_64, including16KiB API35. Coverage includes short and
interrupted I/O, partial append preservation, every probe stat/close failure,
changed file size, descriptor cleanup, retry limits and durable recovery. ARM64
compiles/alignment only. A deliberately wrong fortified partial-write length
fails the Android regression; unchanged controls pass. Invocation-recorded
fixture paths are cleaned on both RED and GREEN. Evidence:
`.cache/storage-fortify*` (ignored).

When manually linking these NDK fixtures, retain the ordinary syscall wraps and
add `--wrap=__read_chk`, `--wrap=__pread_chk`, `--wrap=__write_chk`; the crash
fixture independently needs its fortified-write wrap. Do not disable fortification
to reach a hook. Existing production fuzz/ART/reproduction evidence remains
applicable. Next: continue safe native/platform ownership and restart review;
physical-device custody, authenticated-chain admission and TLS remain gated.

## Production continuation: fault injection must retain fortification — 2026-10-03

The storage fault test now calls the actual fortified entry points with a
smaller declared capacity under short/zero/error injection. Each isolated child
must receive libc's SIGABRT; returning an injected result is a failure. Real
buffers remain large enough and descriptor-1 prevents file access if a guard
regresses. Every child disables core files and is reaped before continuing.

Removing read or positional-read bounds admission fails under both compilers;
removing the fortified-write guard fails on Android. Unchanged controls pass.
Strict Clang/GCC ASan/UBSan/LSan and analyzer checks, complexity and native
API30/35/36 x86_64 runs pass. The original30-second CTest limit remains unchanged
(host0.86/0.87s). ARM64 Android builds/alignment only; the Linux ARM64 UBSan
emulator additionally executes this fixture successfully in1.05s. No production
source, Android artifact or cryptographic semantics changed. Evidence:
`.cache/storage-fortify-bounds*` (ignored).

Separate ARM64 runtime qualification is underway at sourcec82ce8748 in
`.cache/arm64-runtime/`, using cross GCC13.3 and QEMU8.2.2. The UBSan profile
passes119/120 runtime groups; corruption recovery exceeds its30-second deadline
also in isolation. Dynamic ASan fails its minimal startup probe; static ASan
with non-PIE fixtures detects a deliberate overflow, but LSan reports unsupported
process inspection and the broader ASan run encounters multiple unchanged
execution deadlines. These are partial supplemental observations, not completed
ARM64 sanitizer, Android or physical-custody acceptance. Do not weaken deadlines
or substitute emulation for device proof. Continue safe independent engineering.

## Production continuation: admit parameters before full-source copies — 2026-10-03

RED measured an 816000-byte native allocation and 816000-byte JNI copy before
full-source preparation rejected a malformed scalar packet. The lowest owner,
`jni_full_prepare.c`, now checks the immutable previous-array count and existing
bounded parameter decoder first. Wrong packet shapes and invalid scalar ranges
allocate/copy zero source bytes. Valid source capture, exact draft construction,
review ownership/deadlines and cleanup remain unchanged. Multiple malformed
arguments can now report the scalar refusal before a source-element refusal.
No transaction acceptance, monetary rule or cryptographic semantics changed.

Strict Clang/GCC safety suites pass 144/139 groups (58.36s/57.90s), with
ASan/UBSan/LSan, analyzers, provider hashes and complexity caps. The JNI review
fuzzer completes 18869 runs/31s. The native fault fixture and 15 ART
review/preparation/lifecycle tests pass on each API 30/35/36 x86_64, including
16 KiB API 35. ARM64 Android compiles/alignment only. Android JVM/minified/lint,
fixture isolation and alignment gates pass (154 tasks). Source-only independent
release reproduction matches APK SHA-256
`e86ebd175558be59078ea5bfbff26acce0829023a7e6f379d0e5d838d7bd95f3`.
Ignored evidence: `.cache/prepare-admission/`, `.cache/reproduce-prepare-admission/`.
No latency or valid-request speedup is claimed; the measured improvement is
rejected-request allocation/copy work. Native hazard review is recorded.

The separate ARM64 Linux QEMU qualification at source `c82ce8748` is finished:
UBSan passes 119/120 runtime groups in 95.76s; corrupt change recovery also hits
its unchanged 30-second limit in isolation. Static non-PIE ASan+UBSan with LSan
disabled passes 62/120 groups, with 58 deadline failures, in 583.86s. Dynamic ASan
fails its startup probe; static ASan detects a deliberate heap overflow. LSan
reports unsupported process inspection. This is partial supplemental emulation
evidence, not a full sanitizer pass or Android/hardware acceptance. Host LSan
acceptance remains green; no deadlines were weakened. Evidence is retained in
`.cache/arm64-runtime/`. Continue safe independent native ownership/resource and
restart work; TLS and physical custody remain separately gated.
The admission-order mutation restores the original 816000-byte allocation/copy
and fails the new resource assertion under both compilers; unchanged controls
pass. Initial standalone validation commands omitted provider include/config
flags; corrected commands reuse the actual configured provider settings without
suppressing warnings. No product change was needed for those harness errors.

## Production continuation: observe descriptor retirement across exec — 2026-10-03

The existing storage crash fixture now forks an owner that opens the actual
store and execs its own test binary while the directory and lock are live. The
new process observes EBADF for both descriptors; the parent reaps, reacquires
through public creation and cleans only its own fixture. Existing commit and
promotion interruption cases and twelve competing creators still run afterward.
This observes the existing close-on-exec contract without changing production.

RED: independently removing O_CLOEXEC from directory or lock passes the prior
crash fixture but fails the new observer under Clang and GCC. Unchanged controls
pass with ASan/UBSan/LSan; focused CTest takes 0.16s/0.15s under the unchanged
30-second deadline. Both analyzers, strict frame/warning/complexity gates and
architecture/docs/diff checks pass. Actual release-archive fixtures pass API
30/35/36 x86_64, including 16 KiB API 35; Android ARM64 compiles/alignment only.
No new physical-device, cross-emulator exec or power-loss claim is made.

Evidence: `.cache/storage-exec/` (ignored). Product source/APK are unchanged from
`356fb2586`. Full previous safety, JVM, ART and release reproduction evidence
remains applicable. Continue native custody/lifetime and storage restart review;
no upstream write, real wallet, TLS activation or production node operation.

## Production continuation: bound raw QR JNI copy to used pixels — 2026-10-03

RED measured 8388608 bytes allocated/copied for a valid padded QR image whose
layout uses 21609 bytes. The raw `ScanQr` JNI adapter now copies through the last
addressed pixel after the existing C predicate validates the complete array and
layout. Row/pixel strides, maximum input length and decode semantics remain
unchanged. The fixture verifies the same request, zeroized allocation, unchanged
input and refusal of a one-byte-short or oversized array. This is a measured
reduction in native copying for padded raw images, not a camera latency claim;
the separate isolated camera packet path is unchanged.

Full Clang/GCC safety passes 144/139 groups (58.05s/57.73s), including
ASan/UBSan/LSan, analyzers, provider hashes and complexity caps. Independent
mutants that restore whole-array copying, omit the last pixel or bypass the
maximum array length all fail under both compilers. JNI camera fuzzing now also
covers raw scan layouts, allocation faults and partial transfers; 5181 runs/31s
complete without a finding. API 30/35/36 x86_64 pass the native fixture and 12 ART
QR/camera-output/service-retirement tests each. The new independent ZXing case
uses an 8 MiB backing array, row stride 8192 and pixel stride 4; the entire input
remains unchanged. API 35 uses 16 KiB pages; Android ARM64 builds/alignment only.

Android JVM, minified release, lint, fixture isolation and alignment gates pass
(154 tasks). Source-only reproduction matches APK SHA-256
`0f8e3b9e2bd91383046252dd2961e8687a881264945bd953d33ef39881d1bbf3`.
Architecture/docs/diff gates and explicit native hazard review pass. Ignored
evidence: `.cache/scan-prefix/`, `.cache/reproduce-scan-prefix/`. Continue safe
native/platform ownership and recovery work; no real-wallet, TLS or production
node authority has changed.

## Production continuation: canonical ARM64 Linux JNI execution — 2026-10-03

The CMake fake-VM test block can now use explicit target-compatible JNI headers
without requiring JVM discovery. `ZCL_TEST_JNI_INCLUDE_DIRS` applies only to
those existing fixtures; it does not set JNI_FOUND or alter the shipped bridge.
The README records the complete UBSan/QEMU configuration and its limits.

RED: the old configuration ignores the explicit test-header setting and
registers no JNI groups when JVM discovery is disabled; `--no-tests=error`
refuses. GREEN: all 13 registered JNI groups execute as AArch64 through QEMU 8.2.2
with GCC 13.3 and UBSan, passing in 6.63s under unchanged deadlines. This includes
secret-output retirement, camera bounds, wallet storage faults, sync concurrency,
review ownership, address and draft handling. An independent signed-overflow
negative control triggers UBSan. ELF inspection confirms 13 AArch64 executables
and no JVM dependency. Initial manual four-fixture qualification is preserved
in `.cache/arm64-jni/`; canonical evidence is `.cache/arm64-jni-cmake/`.

Default host discovery still passes all 13 groups under Clang/GCC
ASan/UBSan/LSan (1.81s/1.76s). Android release/alignment/fixture-isolation checks
pass (131 tasks); its complete APK remains identical to `58c8b8d81` and the
previous source-only reproduction. No native C, product behavior or deadline
changed. ARM64 Linux emulation remains separate from Android ARM64 compilation,
ART, physical custody and the earlier incomplete broad ASan/LSan qualification.
Continue safe native and lifecycle work from this exact development branch.
The normal safety script now explicitly passes its already detected analysis
headers to both compiler configurations, removing JVM-library discovery as a
reason to omit native JNI fixtures. The repository documentation gate initially
misread an unqualified fixture count as a global node test count; the wording
now states its JNI scope, and the unchanged gate passes.
The updated canonical safety script passes its complete Clang/GCC suites:
144/139 groups in 57.80s/58.88s, including analyzers, sanitizers, provider hashes,
complexity and the unchanged fuzz-profile deadline. The documented ARM64 profile
also completes the full strict build; only its selected JNI runtime set is
claimed as passing here.

## Production continuation: observe private pixels during failed resize — 2026-10-03

QR provider allocation tests now begin resize with an existing marked public
frame, covering shrink, same-size and grow operations. Both resize allocation
failures must preserve the live frame/dimensions and retire any temporary copy;
success must clear the replaced allocation and preserve only the copied prefix.
The existing free observer checks every byte before release. Production still
uses one decoder per scan; this adds no cache or provider change.

RED: removing the failed-copy wipe or old-frame wipe independently passes the
prior fixture but fails the expanded test under both compilers. GREEN: strict
Clang/GCC ASan/UBSan/LSan and both analyzers pass, as do complexity and unchanged
provider hashes. Focused Clang CTest takes 0.08s; ARM64 Linux UBSan executes the
fixture in 0.22s under the original ten-second deadline. Actual release-archive
fixtures pass API 30/35/36 x86_64, including 16 KiB API 35; Android ARM64 builds
and alignment only. Existing production fuzz/JVM/ART/reproduction evidence
remains applicable. Ignored evidence: `.cache/scan-resize/`.

## Production continuation: native MemorySanitizer qualification — 2026-10-03

At source `1c0075d71`, a separate Clang 20.1.2 Linux x86_64 PIE build instruments
the core, all six provider archives and native fixtures with MemorySanitizer
and heap-origin tracking. The deliberate uninitialized heap-read control fails
with the expected diagnostic; initialized startup controls pass. All 135 native
executable tests pass in 17.41s under unchanged deadlines, including the 13 JNI
fixtures and storage exec/decoder resize regressions. Archive symbol inspection
confirms instrumentation in all seven archives. No wallet finding or C change.

README preserves the exact profile. Four script/build contracts are excluded
from this supplementary runtime selection and remain covered by the normal
Clang/GCC safety gate. Evidence is ignored `.cache/msan-probe/`; this qualifies
neither Android/JVM execution nor race, bounds, leak or every possible
uninitialized path. Existing production APK/reproduction and Android evidence
remain applicable. Continue independent native robustness work on this branch.

## Production continuation: real descriptor pressure during storage — 2026-10-03

Added twelve bounded kernel EMFILE/recovery cases for wallet create, read and
pending promotion. Each isolated child leaves exactly 0..3 descriptors available
under its own 64-descriptor limit. Failed reads retain all output sentinels;
operations restore descriptor count. After child exit the parent verifies exact
public fixture contents and retries without resetting/erasing wallet state.
Production C and custody semantics are unchanged.

RED: an EMFILE-specific cleanup leak passes the old storage fault fixture but
fails the new regression under both compilers. GREEN: host Clang/GCC sanitizers,
static analyzers and test complexity cap pass; supplemental MSan takes 0.12s
and ARM64 Linux UBSan/QEMU 0.31s. Release-archive fixtures pass API 30/35/36
x86_64; Android ARM64 builds/alignment only. Release, 16 KiB alignment and fixture
isolation pass (131 tasks). The complete release APK remains byte-identical to
`58c8b8d81` / the prior source-only reproduction. No new ART, physical-device,
ENFILE or disk-exhaustion claim. Ignored evidence: `.cache/storage-limits/`.
Full canonical safety gates: Clang passes 145/145 native/script groups in 61.34s;
GCC passes all 139 other groups but the unchanged fuzz-manifest mutation inner
60-second deadline expires under concurrent validation. Its isolated retry
passes in 57.81s, completing all 140 GCC groups without changing a deadline or
assertion. Provider hashes, analyzers, complexity and repository gates pass.

## Production continuation: Android change-journal interruption coverage — 2026-10-03

The independent change-storage crash fixture also needed Bionic __write_chk
interposition: actual release-archive RED missed its first write interruption.
The repaired test now reaches all 26 create/append interruption boundaries and
12 competing appenders on API 30/35/36 x86_64, including 16 KiB API 35. Original
bounds violations reach libc before partial-write selection; a deliberate guard
removal fails the new SIGABRT control. Production C and archives are unchanged.

Clang/GCC sanitizer fixtures pass in 0.28s/0.32s; supplemental MSan 0.37s and
ARM64 Linux UBSan/QEMU 0.89s. Host and Android ARM64-target analyzers, strict
compilers, complexity and repository gates pass. Android ARM64 compile/alignment
only. Manual linking retains ordinary wrappers and adds --wrap=__write_chk.
RED/GREEN cleanup uses only invocation-recorded fixture paths. Ignored evidence:
`.cache/change-crash-fortify/` and `.cache/change-crash-fortify-mutant/`.
The preceding complete safety/release baseline remains applicable; this does not
establish physical power-loss durability. Continue safe native restart/lifecycle
work without enabling TLS, signing authority or production wallets.

## Production continuation: instrumented uninitialized-read fuzzing — 2026-10-03

At `fe8fb311b`, a separate Clang 20.1.2 MSan/origin-tracking build adds libFuzzer
coverage to every linked native library. All 87 core/provider compile commands
carry both flags, covering every expected library target; all seven archives
contain MSan and coverage references. Existing harnesses are linked manually
with libFuzzer/MSan. The normal ASan/UBSan fuzz admission gate stays unchanged.
An initialized control passes; deliberate uninitialized heap return fails with
MSan's expected diagnostic through the same libFuzzer runtime.

| Existing harness | Runs / duration | Fuzzer seed |
| --- | --- | --- |
| Recovery phrase decode/encode/confirm | 652,009 / 31s | 611006066 |
| Transparent transaction/script codec | 571,801 / 31s | 610678423 |
| QR scanner with public rotated/address/payment seeds | 11,096 / 31s | 618810317 |
| Electrum replies/framing with public network/history/max-frame seeds | 49,135 / 31s | 2801117472 |

No findings. The first three run concurrently with an initial unseeded Electrum
campaign (278,398 cases); the final Electrum campaign adds its intended corpus
and permits the full 16,385-byte frame. Other mutation lengths cap at 16,384.
The seed writer requires a corpus working directory; its initially misplaced
public fixtures were moved by exact filename into ignored evidence. The tracked
workspace is clean apart from this documentation. No secrets or generated
artifacts are committed. README preserves the manual profile; ignored evidence
and source identity are in `.cache/msan-fuzz/`.

These are bounded Linux x86_64 campaigns, not complete parser/cryptographic
proof, Android/physical execution or address/leak/race qualification. No C,
provider, APK or acceptance gate changed. Continue native robustness work from
the current development branch; prior full safety/release evidence still applies.

## Production continuation: admit full-review draft before source copy — 2026-10-03

Full-source review opening now checks its owned draft with the unchanged C codec
and compares the immutable source count before capture/allocation. RED: two
inputs with eight maximum sources allocated816000 bytes and copied816177.
GREEN: allocation0, copy177 (draft only). Unsupported/empty/truncated drafts also
refuse before source copying. Existing full assessment and owner/publication
rules still run; accepted requests add one <=1925-byte parse. No valid-path
latency claim. Multiple invalid fields may now report draft/count refusal before
an uncaptured source-element error; no acceptance predicate is weakened.

Strict GCC/Android initially rejected compiler-inlined frames of4976/4544 bytes.
The admission helper now has its own small C translation unit, preserving the
4096-byte frame limit without suppressions. Late-admission mutations fail both
compilers; all VM exception ordinals, parsed-scratch retirement, exact owned
review bytes and subsequent valid reopening pass. The oversized-source ART case
now uses a matching source count so it still independently exercises the original
size refusal; a separate test covers mismatched count and malformed drafts.

Android JVM/unit, builds, lint, release/alignment and fixture isolation pass
(154 tasks). Actual native fixtures and16 ART tests pass each API30/35/36
x86_64, including16KiB API35. ARM64 Linux UBSan/QEMU0.67s; MSan0.27s; Android
ARM64 compile/alignment only. JNI review fuzzing completes18549 cases in31s.
Fresh source-only release reproduction matches the complete APK:
`2838fc7b3d16ff782ff20ed20387fdeed898cf2ab38018849cb0d1c5a5fd8973`.
Ignored evidence: `.cache/open-admission/`, `.cache/reproduce-open-admission/`.
The complete canonical safety gate passes Clang145/145 in58.40s and GCC140/140
in74.11s, including both analyzers, provider hashes, complexity and unchanged
fuzz-profile deadlines. Architecture/document/diff gates pass. Continue from
this baseline with TLS, physical custody and authenticated-chain gates intact.

## Production continuation: refuse invalid review fee before transport — 2026-10-03

Both review-opening JNI entry points now enforce the existing core maximum-fee
money bound before locking or copying inputs. RED invalid upper fees allocated
17472 bytes/copied4027 in the narrow profile and allocated204000/copied204177
with two full-size sources. GREEN allocates/copies0. No money constant or C
assessment predicate changed; exact MAX_MONEY still opens both profiles.
Out-of-range fee now takes precedence over an active-owner/source-content error;
pending JNI exceptions still take precedence and existing owner state is untouched.

Deterministic regressions cover negative, MAX+1, INT64_MAX and exact MAX; both
compiler guard-removal mutations restore the resource failure. Native exception,
cleanup, lifecycle and fuzz checks pass (16355 cases/31s). JVM direct-entry
boundary tests and Android unit/build/lint/release/alignment/fixture isolation
pass (154 tasks). Actual native fixtures and16 ART tests pass each API30/35/36
x86_64, including16KiB API35. ARM64 Linux UBSan0.81s and MSan0.39s pass; Android
ARM64 compile/alignment only. Fresh source-only complete release APK matches:
`648a91c7d3e56d876be6a8344e4d6789844706039bf12ad4e5ead68bacd5420d`.
Evidence: ignored `.cache/review-fee-admission/`, `.cache/review-fee-admission-full/`
and `.cache/reproduce-review-fee-admission/`. Physical custody, TLS and
fresh authenticated-chain acceptance remain open.
Clang passes all144 other groups but its manifest mutation check initially hits
the unchanged inner60s deadline; isolated retry passes58.49s. GCC passes all139
other groups in43.82s, then its isolated manifest check also times out. The
separate validated `d9dbb710a` checkpoint below removes redundant mutation work;
unchanged canonical manifest gates now pass9.97s/9.95s, completing145/140 groups.
Both analyzers, provider hashes, complexity and repository gates pass.

## Production continuation: bounded manifest-mutation validation — 2026-10-03

Checkpoint `d9dbb710a` preserves the complete emitted-command verification,
checker predicates, all flag/quoting/opt-out/classification mutation assertions,
full no-harness/empty/unrelated-scope checks and every deadline. Each local
flag mutation now consists of its exact emitted entry plus actual valid core
and fuzz-harness controls. The same selected command and expected refusal are
checked without reparsing hundreds of unrelated entries per mutation. Controls
are independently accepted and remain present even when mutating their source,
so other good entries cannot mask a bad command. No production source change.

Measured RED is recurrent inner60s expiry, including an isolated GCC run;
an isolated old Clang pass took58.49s. GREEN complete canonical profile checks
take9.97s/9.95s. Architecture/docs/diff gates pass. Evidence is ignored
`.cache/manifest-mutation-cost/` plus the preceding failure logs. This is test
work reduction, not a sanitizer exemption or weaker instrumentation check.
Continue native wallet hardening with the current validated fee/draft admission,
release reproduction and platform limitations preserved.

## Production continuation: complete JNI fee endpoints — 2026-10-03

The signed production fix `91db31c76` is retained unchanged. Both native review
profiles now explicitly test INT64_MIN in addition to -1, MAX_MONEY+1 and
INT64_MAX, with zero allocation/copy on refusal. A valid synthetic transaction
with its 500-zatoshi fee returned to the first output opens with maximum fee0;
exact MAX_MONEY also opens. Existing deadline tests retain the last accepted
start (INT64_MAX-90000), first overflowing start, final live millisecond and
expiry. Exception handling, native retirement and owner lifecycle coverage pass.
The JVM direct-entry test additionally refuses Long.MIN_VALUE on both APIs.

Focused strict Clang/GCC ASan/UBSan/LSan JNI tests pass (0.20s/0.43s), JVM
UnsignedReviewTest passes, and independent guard-removal controls fail both
compilers while their unchanged controls pass. Both fixture analyzers,
production/test complexity caps, architecture, document counts and diff checks
pass. Evidence: ignored `.cache/review-fee-boundaries/`. Existing release,
ART API30/35/36, ARM64 Linux and MSan observations remain those recorded for
`91db31c76`; this test-only extension makes no new platform/release claim.
SSH verification of that checkpoint succeeds with the configured signing key's
public counterpart. Pending wallet-creation entropy work remains a separate
slice; next validate retirement before public filesystem operations.

## Production continuation: early creation entropy retirement — 2026-10-03

Fee endpoint checkpoint `3d76b3aab589c18c5391230bb62196896dd15bfc` is signed,
pushed to the owned backup branch, and verified equal to its remote SHA.
Upstream remains `3a93e60ebf922af3d119b9facc1d95803f42844b`.

RED observes nonzero native entropy when fresh-wallet creation enters public
storage. JNI now calls an internal consuming adapter that clears its complete
32-byte scratch immediately after private preparation/encoding, before storage.
The public borrowed C API remains unchanged. Final JNI erasure still handles
partial VM reads/exceptions. No derivation,12-word recovery, authenticated record,
no-overwrite or durability predicate changes.

Tests check nonzero full-span retirement on successful creation and repeated
creation refusal, incorrect entropy, wrong entropy length, and random-provider
failure after writing. No file is created on private failures; exact initial
record and ciphertext persist on success/retry. NULL/invalid capacities refuse
without access; SIZE_MAX entropy length wipes an admitted span. Caller originals
remain unchanged. Delaying the wipe until after storage and removing it each
fail deterministic controls under Clang and GCC; unchanged controls pass.

The complete canonical safety gate passes Clang145/145 in26.85s and GCC140/140
in40.69s, including analyzers, provider hashes and production/test complexity.
Android JVM/unit, debug/release builds, lint,16KiB alignment and fixture isolation
pass (154 tasks). Release-archive native fixture plus six storage-admission ART
tests pass each API30/35/36 x86_64; API35 uses16KiB pages. Existing qualified
ARM64 Linux QEMU/UBSan JNI+reservation failure tests pass2/2 in6.79s; existing
qualified MSan fixture passes0.13s. Android ARM64 compile/alignment is not physical
execution or hardware custody proof. Fresh source-only release APK matches:
`19cdfd353a82f2c4a5ee1c43e27e830f8870471115959787fc4cddf53fe17458`.
Evidence: ignored `.cache/create-secret-retirement/` and
`.cache/reproduce-create-secret-retirement/`; manual hazard review appended.
JNI storage fuzzing completes31348 cases in31s without a finding. Architecture,
document-count and diff gates pass. Continue independent secret-retirement and
JNI/storage work; hardware-backed custody, TLS and authenticated-chain acceptance
remain explicitly open.

## Production continuation: retire signing context before public work — 2026-10-03

Creation-retirement checkpoint `4c6c41cfbc6a59a49ba98c9c6a39d1e455c1e6c1` is
signed and backed up with exact local/remote SHA equality. Upstream remains
`3a93e60ebf922af3d119b9facc1d95803f42844b`. Wallet-record JNI inspection finds
bounded local references and intact secret retirement; no defect/change there.

RED signing fault fixture reaches first public normalization with the randomized
context still allocated. The single context teardown now runs immediately after
signing/scalar erasure. Public encode/verify use the pinned provider's immutable
static context; private operations retain the randomized context and constructor
self-test. No change to signature bytes, nonce bounds, verification or authority.
The regression checks freed/erased allocation and static context at normalization.
Delaying teardown restores failure on both strict sanitizer compilers; controls
pass. Existing20 provider/allocation/nonce failure modes still preserve outputs.

Canonical native safety passes Clang145/145 in24.61s, GCC140/140 in53.02s;
provider hashes, analyzers and complexity caps pass. Existing deterministic
signature comparisons and independent OpenSSL oracle pass. Fuzzing completes
12522 cases/31s without a finding. Android unit/JVM, builds, lint,16KiB alignment
and fixture isolation pass (154 tasks). Qualified MSan signature/failure fixtures
pass2/2 in0.16s; ARM64 Linux QEMU/UBSan passes2/2 in1.77s. Android ARM64 remains
compile/alignment evidence only. This is earlier retirement, not a measured
throughput or peak-memory optimization. Evidence is ignored
`.cache/sign-context-retirement/`; manual C hazard review appended.
Actual release-archive signature and fault fixtures pass API30/35/36 x86_64,
including the16KiB API35 image. Source-only release reproduction matches the
unchanged full APK `19cdfd353a82f2c4a5ee1c43e27e830f8870471115959787fc4cddf53fe17458`;
this internal signing primitive still has no exposed send path. Architecture,
document and diff gates pass. Continue with independent storage interruption and
resource handling; custody, fresh-chain authorization and TLS gates remain open.

## Priority switch: publish reusable C work upstream — 2026-10-03

User replaced autonomous Android hardening with small upstream PR publication.
Current signed/backed-up Android HEAD is
`6fdcb046825504684133b8cffe3fd952ae986183` (exact remote equality verified).
The unfinished file-size-limit fixture remains uncommitted and preserved; only
its initial Clang test passed. Do not resume that independent research while
publication slices remain. Inventory, classifications and source provenance are
in UPSTREAM_PUBLICATION.md. Publication uses isolated current-main worktrees,
CesareFI/z23 branches and PRs to z23c/z23:main. Reviewer identity verified as
RhettCreighton. Initial upstream open-PR list is empty. No PR yet; first candidate
adapts mnemonic temporary retirement to the existing upstream wallet owner.

## 2026-10-03 — reusable C upstream candidates prepared

Publication priority supersedes further Android research. The four independently
signed candidates, exact upstream base, inventories and source provenance are in
UPSTREAM_PUBLICATION.md. Each candidate passed all215 repository lint gates,
focused canonical tests, compiler lanes and applicable sanitizer/mutation proofs.
Base58 also has fresh host measurements and bounded fuzz evidence; descriptor
coverage is a test-only port, not a change to upstream durability policy.

Publication is pending one authority decision: the installed upstream hook refuses
fork branches with `remote-ref-not-main`, while this mission forbids main pushes
and bypassing checks. The normal push was refused; no override, policy edit or
alternate publication transport was used. No PR/CI/reviewer request exists yet.
Descriptions and exact evidence remain in the isolated sibling publication
checkout. Resume by resolving that explicit hook conflict, fetch current main,
recheck open PRs, then publish the four exact candidate branches to CesareFI/z23
and open separate z23c/z23 PRs, requesting verified reviewer RhettCreighton.
Do not rerun completed tests without changed source or a new validation reason.
The unfinished native storage-limit test remains dirty and excluded throughout.

### Publication continuation: broader inventory and proof identity

The owner-path audit found three existing native commits missed by the initial
app-path inventory. The selected set is now five independent slices; Base58
`987d226c3` is preserved but superseded by `be78a2bf9` (consumed-prefix encoder),
and `d95e540be` ports existing xprv retirement. Focused validation is recorded in
UPSTREAM_PUBLICATION.md. Do not claim publication readiness from earlier lint
alone: the storage candidate has its exact native PASS receipt; the root-run
mnemonic universal proof failed17/1236 groups, including explicit non-root UID
requirements. An isolated copy under existing UID1000 is being qualified before
retrying broader proofs. The fork-only hook exception remains unresolved.

### Publication proof checkpoint

All five selected slices now pass full215-gate lint. Base58 prefix `be78a2bf9`
has its exact native PASS receipt (35/35 groups, zero skips), alongside storage
`b25e546ba` (20/20). Non-root mnemonic qualification reduced the universal run
from17 failures to2: absent reflex fixture images and an overlong socket path.
Both groups pass on unchanged upstream through a short isolated path; the
short-path retry has its fixture present and lint passing, with runtime pending.
Key-scratch and xprv exact proofs are being prepared independently under the same
bounded non-root environment. UPSTREAM_PUBLICATION.md records identities and
local evidence locators. No test/policy/host permission was weakened. No fork
push or PR exists while the explicit main-only hook conflict remains unresolved.
