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
