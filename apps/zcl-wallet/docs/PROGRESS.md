# Development record

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
fuzzer completes21,928 executions in121 seconds without a finding; its110-case
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
assertions. The extended review fuzzer completes327,566 executions in121 seconds
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

The OpenSSL-enabled fuzzer completes36,242 runs in121 seconds without a finding.
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

The final OpenSSL differential script fuzzer completes1,792,794 cases in121
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
completes71,385,647 cases in121 seconds without a finding. The current minified
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
The new bounded filesystem fuzzer completes305,619 cases in121 seconds without
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

2026-09-14: secret JNI outputs now complete array allocation/acquisition before
writing entropy or recovery words. Previously an injected region-write exception
after copying left secret data in an unpublished managed array, beyond the
caller's cleanup. A new regression reproduces that fault-model gap; this is not
an observed ART vulnerability. Copied elements are now committed, erased while
owned, then released without overwriting the result. Direct elements release
once. Original exceptions remain pending, and failed acquisition leaves only
zero output storage. No JNI signature, key derivation, custody policy, disk
format, network or consensus behavior changes.

The existing fake-VM unit/fuzzer now checks both copy choices, NULL acquisition
with and without exceptions, unexpected non-NULL acquisition with an exception,
exact output, no surviving element pointer, complete VM-copy erasure and native
encode/decode scratch cleanup. Eight removed-clear/release/commit mutations fail
intended assertions. Bounded fuzzing completes 48393 cases in 121 seconds without
a finding (217-byte input limit, five-second cases, 512 MiB RSS cap).

Clang/GCC analysis and production/test complexity limits 10/15 pass without
suppression. All 89 ASan/UBSan/LSan groups pass. All 118 JVM tests, Android debug
and unsigned release builds, debug/release lint, fixture isolation, 16 KiB native
alignment and architecture checks pass. Fresh API 30 and API 36 x86-64 emulators
with CheckJNI enabled each pass 24 key/record/secret-view/authentication-window
tests, including every supported entropy size with nonzero public fixtures.
Two additional API 36 wallet lifecycle tests pass in 3.096 seconds: repeated
foreground replacement remains bounded, and a pause-rendering failure still
closes the worker and clears queued recovery input. These checks qualify neither
physical ARM64 behavior nor successful hardware-authenticated custody.

The 430-byte native character staging array is removed. With Clang 20 -Os and
stack protection, the recoveryPhrase frame falls from 744 to 344 bytes. The
translation unit's object text grows by 453 bytes to handle element ownership;
this is not a total APK/RSS improvement claim. VM acquisition can allocate one
bounded copy, at most 430 bytes, and its failure is checked. The detailed hazard
review and JNI ownership sources are in C_SAFETY_REVIEW.md and KEY_DESIGN.md.

Exact logs, initial failures, mutations, source, stack measurements and artifact
hashes remain in the isolated development worktree's
`.cache/android-wallet/jni-secret-transfer-20260914/`. The unsigned release APK
SHA256 is `de0d00cf58f3d12019b17815e50f90c35785a1f18922f50d315259356acd63d2`.
Both fresh emulators use the qualified reaping wrapper and receive graceful
shutdown; the pre-existing emulators and unfinished camera/storage work remain
untouched. The first instrumentation invocation used an incorrect test package
name and refused before running tests; corrected explicit-package runs above
passed. TLS remains quarantined and physical custody acceptance remains open.

2026-09-14: OS entropy-source qualification now observes the full native scratch
lifetime and clearing, in addition to exact output and bounded syscall behavior.
The old tests pass when scratch erasure is deliberately removed; the updated
suite rejects that mutation. Production randomness and every shipped byte remain
unchanged. The test-only reader/model enumerates 65603 synthetic cases, including
all byte-sized input claims/actions and the exact final permitted read attempt.
Partial/error writes cannot publish caller bytes or leave an unretired scratch
owner. The existing actual-OS read smoke tests remain in the unit.

Eight production mutants fail intended assertions. Clang/GCC static analysis
passes both unit/fuzz profiles, and the production/test complexity caps stay
10/15. An initial GCC finding on a redundant reference staging copy was resolved
by simplifying the reference; the finding remains in the evidence directory.
All 89 sanitizer groups pass in 64.75 seconds; the final focused RNG group
passes in 0.08 seconds. The final bounded fuzzer completes 16082900 cases in
121 seconds without a finding (129-byte input cap, five-second cases, 512 MiB
RSS cap; observed 269 MiB). Its SHA256 is
`6432d291fad35dd98f79c398d07b5ea138827cab2a1722bf68600d5d6ea9b609`.

Android/JVM builds/tests, debug/release lint, fixture isolation, native alignment
and architecture checks pass. Debug, instrumented-test and unsigned release
APKs compare byte-identically to the JNI transfer checkpoint. Its exact API
30/36 device tests remain applicable; no new emulator was required. The original
three emulators and 26 ADB zombies remain untouched. Exact source, logs, analyzer
finding and mutation/fuzz evidence are in the isolated worktree's
`.cache/android-wallet/random-erasure-20260914/`.

Continuation: work remains on `agent/android-security-hardening-20260914` in
`/tmp/z23-android-security-hardening-20260914`. JNI checkpoint `a86fe1322` is local;
the configured remote account's development-branch push was rejected with HTTP
403. Main was never pushed, and no remote identity or hook was changed. Preserve
the independent unfinished camera/storage worktrees. Continue security and
bounded-memory qualification locally; TLS remains quarantined, physical custody
is unqualified, and signing/broadcast remain disabled in the launcher.

2026-09-14: backup words are published only after both confirmation and cancel
controls exist. The screen builder now owns incoming characters from entry and
clears them if the previous screen cannot clear or construction throws. The
recovery view's existing concealed-render and cleanup contract handles the final
word publication. Normal layout, key handling, authentication and native code
are unchanged; no extra view, array, callback, timer or background owner is added.

Four instrumented regressions fail against the prior builder: failure while
adding either control, failure while clearing the previous display, and a text
observer seeing recovery words before the controls exist. Final tests also
verify a retry after construction failure and actual Activity recreation: the
old owned characters clear and the replacement does not restore the backup.
All 26 backup/secret-view/native-key cases pass on isolated API 30 and API 36
x86-64 emulators. The fixtures use three-character public markers in the
nonexported debug display host; they do not open a wallet or bypass hardware
custody. This is debug screen/lifetime evidence, not a hardware-authenticated
create/restore acceptance or physical-device qualification.

Android/JVM, debug/release builds and lint, fixture isolation, native alignment
and architecture checks pass. Four relevant native sanitizer groups pass in
0.75 seconds. Both native libraries compare byte-identically to the prior
checkpoint; no native fuzzer rerun is needed for the Kotlin-only production
change. The debug APK remains 3940610 bytes. The new unsigned release APK SHA256
is `506978f30d8249c196a78c4ce97d80083d3c146799913da479f0fb015a1aefdc`.
Source, before/after failures, APKs and logs remain in the isolated worktree's
`.cache/android-wallet/backup-publication-20260914/`. The two owned emulator
launches complete graceful shutdown through the qualified reaping wrapper;
existing emulators and unrelated dirty camera work remain preserved.

2026-09-14: long mnemonic keys are normalized once before PBKDF2 instead of
repeating the same SHA-512 normalization in every HMAC. All 2048 rounds, the
exact 64-byte seed, checksum/ASCII rules and recovery profile remain unchanged.
The new 64-byte native scratch is always erased, including a provider failure
after a partial write. There is no new allocation, managed secret, cached state
or API. The change uses the existing reviewed SHA-512 provider.

Three mandatory fixed vectors cover 127/128/129-byte mnemonics. The old tests
miss an exactly-128-byte normalization mutation; the new tests reject it.
Seven targeted mutations are detected, including omitted/partial erasure and
altered rounds. The optional OpenSSL oracle independently checks 81 seeds with
empty, six-byte and maximum-length passphrases; the existing independent oracle
also checks 96 complete receive/change addresses. The new oracle fuzzer compares
seeds from original mnemonic bytes and checks failure atomicity for unsupported
passphrases. It completes 9532 runs in 121 seconds without a finding, using a
161-byte input cap, five-second cases and 512 MiB RSS cap (333 MiB observed).
An initial GCC staging-copy finding led to removing that temporary array;
final unit/fuzz profiles pass strict Clang/GCC analysis without suppressions.

All 89 native sanitizer groups pass in 63.98 seconds. All 118 JVM tests,
Android debug/release builds and lint, fixture isolation, native alignment and
architecture checks pass. All 29 selected key/record/secret-display/authentication
tests pass on both isolated API 30 and API 36 emulators. The release native
mnemonic-vector executable also passes on both. ARM64 is compiled, not executed;
no physical-device custody or full-wallet acceptance is inferred.

On the isolated x86-64 API 36 emulator, a release-code benchmark interleaves
before/after/after/before runs, using 14 samples of 128 operations per profile
and phrase length. Median thread CPU per 179-byte phrase falls from 4794644 ns
to 3195572 ns (33.35%); the 89-byte phrase is essentially unchanged at 3241132 ns
versus 3213382 ns. Each iteration checks its exact deterministic seed. This is
an emulator microbenchmark, not physical-phone latency, battery or UI evidence.
NDK release-profile combined mnemonic/PBKDF2 frames grow 120 bytes on x86-64
and 144 bytes on ARM64; provider/caller frames are additional. Object text grows
163/236 bytes. The debug APK grows 7136 bytes to 3947746; the unsigned release
APK is 613111 bytes, SHA256
`d65265def068e44a7fac6c67745012af6503a4e7d16925b57d319de8e2d761f0`.

The full hazard review is in C_SAFETY_REVIEW.md. Source, benchmarks, boundary
generation, failed mutations and exact APK/native hashes are retained under
`.cache/android-wallet/seed-profile-20260914/` in the development worktree.
Both owned emulators shut down gracefully; existing devices and unrelated dirty
work remain untouched. Origin main was fetched and has no new wallet changes.
Work remains local because the earlier development-ref push was rejected with
HTTP 403; no main push or merge occurred. TLS remains quarantined and hardware
custody qualification remains open.

2026-09-14: recovery delivery now retires both queued input references when
cancelled, rejected or claimed. The old owner cleared cancelled characters but
left the array and receiver in the queued holder, which could retain the old
Activity until queue disposal. Claiming also retained those references after
successful delivery. The new implementation clears them before invoking the
receiver; successfully transferred words remain intact, and a failed receiver
still clears them. It retains the one-pending-delivery bound, identity checks,
lock scope and nonblocking close. No buffer, task, thread or normal-path object
allocation is added.

Three JVM regressions fail the old implementation. They observe strong input
references directly rather than depending on garbage-collector timing, and
cover cancellation before dispatch, retirement before a successful/failing
receiver, reentrant close and an executor throwing after enqueue. Ordinary
rejection and fatal allocation-error models both preserve the original failure,
clear the cancelled input and allow a new delivery; the old queued task cannot
claim that replacement. Existing posting/cancellation concurrency tests remain.
All 121 JVM tests pass. The Android main-queue fixture now observes the actual
ART holder before the queued callback can run, verifies both references retire
on close and confirms no stale delivery. All 31 selected key, record, secret
display, backup, authentication-window and worker tests pass on both isolated
API 30 and API 36 emulators. This does not qualify physical hardware custody.

The minified release's mapped instructions retain both NULL stores on claim,
rejection and close; bytecode inspection confirms no extra success-path object
allocation. Android builds, debug/release lint, fixture isolation, native
alignment and architecture checks pass. Both ABI libraries are byte-identical
in debug and release to the preceding seed checkpoint. Four focused native
sanitizer groups pass in 1.49 seconds; its full sanitizer/fuzz evidence remains
applicable to the unchanged native bytes. Debug/release APK sizes remain
3947746/613111 bytes. The unsigned release APK SHA256 is
`3dba23b1c3d62772020b7f240f630ae717c4538774707f283444e08398027ef9`.

Source, baseline failures, final tests, bytecode/mapping and exact artifacts are
in `.cache/android-wallet/phrase-delivery-20260914/`. An initial instrumented
test compile failed for a missing assertion import; the corrected build passes
without suppression. Owned emulator launches shut down gracefully. Existing
devices, the original 26 ADB zombies and unrelated dirty work remain preserved.
The development branch remains local after the earlier HTTP 403 push refusal;
no main push, production operation or custody-policy change occurred.

2026-09-14: BIP32 failure tests now observe complete clearing of master/child
digests, child derivation input and the child private result. They record fixed
integer address stamps, validate the real zeroizer's live span and check owner
retirement without dereferencing expired stack memory. A failed-HMAC fixture
writes nonzero public markers before refusing. Eighteen dedicated cases bind
success/failure behavior across ordinary and hardened children and invalid
zero/order/negative scalar results. The separate master result copy and provider
internals are not claimed as independently observed by this fixture.

All six omitted/shortened-clear mutations pass the former tests and fail the
strengthened owner/span assertions. Diagnostic backtraces confirm those sites
without displaying argument values. Initial fixture complexity exceeded the
test cap; extracting synthetic digest construction restored the unchanged
15 limit. Strict Clang/GCC analysis and all 89 ASan/UBSan/LSan groups pass, the
latter in 63.79 seconds. Android/JVM builds/tests, debug/release lint, fixture
isolation, native alignment and architecture checks pass. Production source and
all three APKs are byte-identical to the previous checkpoint, so its exact
API 30/36 tests and the unchanged native fuzz campaigns remain applicable.
No emulator or operator state was touched for this test-only change.

The explicit hazard review is in C_SAFETY_REVIEW.md. Source, six pairs of
before/after mutation runs, backtraces, stack/metadata budgets, the initial
complexity failure and final hashes remain in
`.cache/android-wallet/bip32-erasure-20260914/`. The development branch remains
local after the earlier HTTP 403 push refusal; main was fetched and left intact.

2026-09-14: native public-key conversion rejects structurally malformed calls
before allocating or randomizing an EC context. A shared private check retains
the same NULL/32-byte-secret/33-byte-output rules at both conversion boundaries.
Valid requests still use the full existing blinding, scalar, serialization and
cleanup paths. A malformed argument now wins over a simultaneous injected
provider fault, avoiding both needless work and a misleading resource error.

The new regression fails against the old entry. All 54 NULL/length/capacity
cases now observe zero context-provider requests and unchanged output across
normal and five failing provider modes. Existing allocation/erasure, provider
failure and BIP32 vector checks remain green. All 89 ASan/UBSan/LSan groups pass
in 64.61 seconds; Clang/GCC analysis, unchanged 10/15 complexity caps, JVM tests,
Android builds/lint, fixture isolation, alignment and architecture gates pass.
The JNI key fuzzer completes 51607 cases in 121 seconds without a finding
(217-byte input cap, five-second cases, 512 MiB RSS cap; 52 MiB observed).

All 31 selected Android tests pass on both API 30 and API 36 isolated emulators.
The exact release-archive native BIP32 executable also passes its 17 published
paths, bounds and blinding checks on both. ARM64 is compiled only. NDK release
stack budgets are unchanged; source object text grows 57 bytes on x86-64 and
52 on ARM64. This proves avoided provider work for invalid native API calls,
not a measured improvement in ordinary wallet latency or phone battery use.

The incremental debug APK unexpectedly reached 4334210 bytes although native
payload growth was only 400 bytes across both ABIs. Its prior package output
and incremental state were preserved, then regenerated through the normal
Gradle packaging task. Fresh debug packaging is 3559725 bytes, with every entry
byte-identical to the larger archive; the size difference was unused ZIP space.
The compact artifact passed all 31 Android tests again on both emulators.
Fresh release packaging retained its exact 613127-byte APK, SHA256
`8ce4c1bf523296e0497b3ab7e1ede0a0c8135f4b3467bc5fe8ba06a36dbdebc7`.
Use fresh packaging or compare payloads before attributing incremental APK
size changes to source changes. No packaging rule or app behavior was weakened.

The full hazard review is in C_SAFETY_REVIEW.md. Initial/final APKs, content
hashes, baseline failure, budgets, fuzz evidence and accepted artifacts remain
in `.cache/android-wallet/public-key-preflight-20260914/`. Owned emulators shut
down gracefully; unrelated devices, dirty work and production state remain
untouched. Work remains on the development branch, locally checkpointed after
the earlier HTTP 403 remote push refusal.

2026-09-14: platform sessions prepare their bound secret-clearing callback
during construction, before owning secrets or admitting a worker. Previously
`close()` allocated that callback before requesting executor shutdown. The
change removes that allocation site from both ordinary and minified release
bytecode. It adds one stored reference per session and replaces repeated
close-time callback allocation with a single construction-time allocation.
This is evidence of a removed failure point, not an observed real VM OOM or a
claim that every framework shutdown operation is allocation-free.

A new instrumented regression fails the old implementation because no prepared
callback exists. It observes the actual callback identity, stages only public
marker bytes while a worker is active, verifies repeated close preserves them
until that worker finishes, and then verifies complete clearing and finalizer
retirement. Its cipher is never initialized or used; the unique fixture path
is never opened. No key creation, wallet access, authentication bypass or
hardware custody claim is involved.

All 121 JVM tests pass. Debug/release builds and lint, fixture isolation,
native alignment and architecture gates pass. Both isolated API 30 and API 36
emulators pass all 32 selected secret-lifecycle tests plus both existing
pause/resume and cancellation journeys. The owned emulator launches shut down
gracefully with exit zero. Both ABI libraries remain byte-identical to the
preceding checkpoint in debug and release; its 89 native sanitizer groups,
strict analysis and bounded JNI fuzz campaign remain applicable.

Fresh debug/release APK sizes remain 3559725/613127 bytes. The unsigned release
APK SHA256 is
`bfad07116415f30f64110b6b5328f88b7ed27fca54001d8bfe3eb9cc965ddfb7`.
Before/after bytecode, release mapping, baseline failure, tests and exact
artifact hashes remain in `.cache/android-wallet/session-close-20260914/`.
Origin main was fetched and has no new wallet changes. Work remains locally
checkpointed on the development branch after the earlier HTTP 403 push refusal;
existing devices, unrelated dirty work and production state remain untouched.

2026-09-14: recovered the unfinished camera canvas-restoration patch and its
two public instrumented regressions from the earlier Android worktree. The
original dirty files remain unchanged, verified by before/after SHA256; their
exact diff is retained with this checkpoint. The isolated development branch
now restores the caller's canvas in finally after applying preview transforms.
No drawing exception is swallowed, and no image allocation, camera owner,
frame bound or normal rendering transform changes.

The recovered failure test first runs against the preceding debug APK and fails
with save count 3 instead of 2 after Android's actual recycled-bitmap refusal.
The fixture uses an unattached view and owned software canvas with an existing
save level, translation and clip. The fixed code restores all three. Its
success companion checks four rotations and both facing modes, uploaded public
pixels, untouched pixels outside the clip, and erased upload scratch. This
does not independently qualify physical camera orientation or optics.

All 14 selected preview, camera-start failure, QR and scanner-state tests pass
on both isolated API 30 and API 36 emulators. Both also pass three real Camera2
tests: three background/resume cycles, Activity recreation with explicit
restart, and cancellation while an open callback is pending. These groups
complete in 50.842/51.554 seconds respectively. The ordinary and minified release
bytecode both retain canvas restoration on the exception path; no new draw-time
object allocation appears. The owned emulators shut down gracefully.

All 121 JVM tests, debug/release builds and lint, fixture isolation, native
alignment and architecture checks pass. Both ABI libraries are byte-identical
to the preceding checkpoint, retaining its native sanitizer and fuzz evidence.
Fresh debug/release APK sizes remain 3559725/613127 bytes; the unsigned release
SHA256 is `843ed50500f3f071aab2f9692905865df24998eefd9e532c5087f2f8a2754672`.
The recovered diff, baseline failure, final tests, bytecode, mapping and exact
artifacts remain in `.cache/android-wallet/canvas-restore-20260914/`. Origin
main was fetched without new wallet changes. The development branch remains
local after the earlier HTTP 403 push refusal; production and unrelated work
remain untouched.

2026-09-14: HMAC failure tests now bind erasure to the actual observed key block,
inner digest, hash-result destination and SHA512 context. The existing fixture
counted clearing calls but accepted all four variants that shortened one of
those clears by a byte. All four now abort at the exact-span assertion; bounded
backtraces verify the intended site with argument values hidden.

The observer stores only integer address stamps and counts, then checks the
real zeroizer's live argument. It never reads expired stack memory or retains
secret contents. Twenty failure positions plus short/long-key successful cases
require exact acquire/retire counts. SHA512 finish now writes a public nonzero
marker before a synthetic provider failure, and failed caller outputs remain
unchanged. An early failure may precede provider observation of the block or
inner digest; the claim covers the spans actually observed, not every possible
provider/VM/hardware copy. No production implementation changes.

The full hazard review is in C_SAFETY_REVIEW.md. Strict Clang/GCC fixture and
production analysis, unchanged complexity caps and all 89 ASan/UBSan/LSan groups
pass; the full registered run takes 63.84 seconds. All 121 JVM tests, Android
builds/lint, fixture isolation, native alignment and architecture pass. All
three APKs remain byte-identical to the preceding camera checkpoint, preserving
its exact device evidence and the unchanged native fuzz evidence. No emulator
or operator state is touched by this test-only change.

Source, four pairs of mutation runs, diagnostic backtraces, measured test
budgets and exact artifacts remain in `.cache/android-wallet/hmac-erasure-20260914/`.
Origin main was fetched without new wallet changes. The development branch
remains local after the earlier HTTP 403 remote push refusal.

2026-09-14: camera JNI allocates and clears the exact packet size reported by
the C sampler instead of the 147461-byte maximum for every frame. The shared
sampling geometry supplies a bounded, failure-atomic size query; packing still
independently validates source bounds and capacity before copying. No packet
format, sample coordinate, decoder, pointer lifetime or camera-owner rule changes.

The new allocator regression fails the old code and now observes six exact
valid allocation sizes plus no allocation for two invalid sampled shapes.
For 640x480, the measured request falls to 76805 bytes: 70656 fewer temporary
bytes (47.915 percent). Full allocation erasure, failed allocation, pending JNI
exceptions, partial VM copies, input preservation and guarded output still pass.
An independent enumerating sampler also verifies the new size query across
1200 layouts and malformed boundaries. The expanded camera fuzzer completes
3831 runs in 121 seconds with no finding, under a 1048584-byte input cap,
five-second cases and 512 MiB RSS cap (257 MiB observed).

All 89 native ASan/UBSan/LSan groups pass in 64.56 seconds. Strict Clang/GCC
analysis of production and edited fixtures, unchanged complexity caps, all 121
JVM tests, Android builds/lint, fixture isolation, native alignment and
architecture pass. All 14 selected preview/start-failure/QR/state tests and
three real Camera2 background/recreation/cancellation tests pass on both
isolated API 30 and API 36 emulators. A camera-vector executable linked against
the exact release archives also passes on both; ARM64 is compiled only.

Release-profile packing frames remain unchanged; JNI packing grows 16 bytes
on ARM64 and remains unchanged on x86-64. Combined source object text grows
216/287 bytes respectively. Fresh debug/release APKs grow 544/288 bytes to
3560269/613415. The unsigned release SHA256 is
`6002f632c3906183424c5f6e036656375ffb82e419be0c80117db0305bdb1985`.
This measures temporary allocation/clearing bytes, not physical-phone RSS,
latency, battery, optics or hardware custody. No new worker, cache or native
handle is introduced.

The full hazard review is in C_SAFETY_REVIEW.md. Baseline failure, an initial
test compile diagnostic and its correction, final tests, native budgets,
fuzz corpus and exact artifacts remain in
`.cache/android-wallet/camera-allocation-20260914/`. Both owned emulators shut
down gracefully with exit zero, leaving the original devices and 26 ADB zombies
unchanged. The earlier dirty camera files remain byte-identical in their source
worktree. Main was fetched without new wallet changes; work remains on the
local development branch after the earlier HTTP 403 push refusal.

2026-09-14: optional host HMAC verification now compares arbitrary bounded
binary keys/messages with independent OpenSSL 3.0.13. The deterministic test
covers every supported key length, nine message-length boundaries and three
patterns: 6939 exact comparisons with input-preservation and output-canary
checks. It shares no app normalization, padding or hash implementation when
computing the expected MAC. Existing RFC vectors and erasure tests remain.

Four temporary variants—incorrect normalization at 128 bytes, truncating a
512-byte message or 256-byte key, and clearing a borrowed 256-byte key—pass the
prior focused HMAC executable but fail the new oracle with its explicit mismatch
result. This does not claim they all evade every existing wallet test. The new
differential fuzzer completes 5824641 runs in 121 seconds without a finding,
with a 770-byte input cap, five-second cases and 512 MiB RSS cap (279 MiB observed).
The four independent HMAC/seed/address/change-state groups pass in 5.75 seconds.

All 89 ordinary ASan/UBSan/LSan groups pass in 63.99 seconds. Strict Clang/GCC
checks of both new compilation profiles, unchanged complexity caps, all 121 JVM
tests, Android builds/lint, fixture isolation, native alignment and architecture
pass. Production source and all three APKs remain unchanged, preserving the
preceding device evidence and 613415-byte unsigned release. OpenSSL is linked
only into the existing optional host oracle profile, never Android. README
instructions expose the registered test and bounded fuzz target.

The full hazard review, scope limits and measured stack frames are recorded in
C_SAFETY_REVIEW.md. Source, four pairs of mutation runs, fuzz corpus, reports,
exact test/fuzzer/libcrypto hashes and unchanged APK identities remain in
`.cache/android-wallet/hmac-oracle-20260914/`. No emulator or operator state was
touched. Main was fetched without new wallet changes; the branch remains local
after the earlier HTTP 403 remote push refusal.
