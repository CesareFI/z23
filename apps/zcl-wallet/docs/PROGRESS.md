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
