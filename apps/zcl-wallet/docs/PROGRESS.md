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
