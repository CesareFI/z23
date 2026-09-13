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
