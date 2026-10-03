# Zclassic Android wallet

Development work for the existing Zclassic chain and ZCL. Core implementation
is portable C17, with strict memory-safety controls and a thin Android
Kotlin/JNI UI/platform adapter. It links no Z23 node code and changes no
consensus rules. The user's language correction authorizes C17 instead of the
repository's C23 convention. Rust is not used.

This is unfinished development software. Do not fund development addresses or
import a key controlling real funds. No signing or broadcast is enabled yet.

## Build

JDK 17, CMake 3.22.1, Android NDK 27.2.12479018, Android SDK platform 36 and
build tools 35.0.0 are required. The app targets Android 11 and later. Run from
this directory after installing the SDK and setting `ANDROID_HOME`:

```sh
./gradlew :wallet-core:test :android-app:assembleDebug :android-app:lintDebug
```

The initial Kotlin parser tests now exercise the C implementation through JNI;
the Android project and public fixtures are retained. The verified Gradle
wrapper, dependency lockfiles and SHA-256 verification metadata are present.
Checksums pin the first resolved HTTPS artifacts; they are not independent
publisher-signature verification. An offline build exercised their enforcement.
Android hardware-backed custody requires device acceptance; host unit tests do
not prove device security.

Verify fixture separation in the built debug, test and unsigned release APKs:

```sh
./gradlew :android-app:checkFixtureIsolation
```

This task is also part of `:android-app:check`. It requires a nonexported debug
display host, excludes that host from the release manifest, and keeps public
response frames only in the test APK. Negative manifest fixtures must refuse;
the APKs themselves are never modified. Reports and exact APK hashes are saved
under `android-app/build/reports/fixture-isolation`. This does not run device
instrumentation or qualify hardware custody.

An explicit [custody qualification build](docs/DEVICE_QUALIFICATION.md) uses a
separate test-only application identity and output directory. It disables wallet
release builds in that mode; physical-device custody remains unproven.

Native builds enable the NDK's flexible page-size support. Verify the actual
debug/release APK alignment and both packaged libraries' ELF LOAD segments:

```sh
./gradlew :android-app:checkNativeAlignment
```

This also runs in `:android-app:check`. It requires 16 KiB-compatible segment
alignment and file/virtual-address offsets, uses the SDK's APK alignment check,
and refuses an unexpected native-library inventory. Reports and APK hashes are
saved under `android-app/build/reports/native-alignment`. Eleven public-fixture
JNI/storage/QR tests also pass on an API35 x86_64 16 KiB emulator; the preserved
4 KiB library fails to load there. This does not qualify arm64 hardware or
hardware-authenticated custody. Runtime details are in the work log.

### ARM64 Linux native JNI fixtures

The C failure fixtures use a fake VM and need JNI C headers, not a JVM library.
`ZCL_TEST_JNI_INCLUDE_DIRS` supplies explicit, target-compatible header paths for
these tests; default host discovery is unchanged. It does not enable the shipped
JNI bridge or qualify a real VM. The normal safety script supplies the same JDK
headers it already uses for analysis, so JVM-library discovery cannot silently
omit these fixtures. With cross GCC, the ARM64 Linux sysroot, QEMU
user emulation and JDK 17 headers installed, run from this directory:

```sh
cmake -S native -B native/build/arm64-jni \
  -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
  -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc \
  '-DCMAKE_CROSSCOMPILING_EMULATOR=/usr/bin/qemu-aarch64;-L;/usr/aarch64-linux-gnu' \
  -DCMAKE_BUILD_TYPE=Debug '-DCMAKE_C_FLAGS_DEBUG=-O2 -g' \
  '-DCMAKE_C_FLAGS=-fsanitize=undefined -fno-sanitize-recover=all -fno-omit-frame-pointer' \
  -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=undefined \
  -DZCL_SANITIZE=OFF -DZCL_FUZZ=OFF -DZCL_JNI=OFF \
  -DZCL_ORACLE=OFF -DZCL_TLS_REVIEW=OFF \
  -DCMAKE_DISABLE_FIND_PACKAGE_JNI=TRUE \
  '-DZCL_TEST_JNI_INCLUDE_DIRS=/usr/lib/jvm/java-17-openjdk-amd64/include;/usr/lib/jvm/java-17-openjdk-amd64/include/linux'
cmake --build native/build/arm64-jni -j8
ctest --test-dir native/build/arm64-jni -R '^wallet_jni_' \
  --no-tests=error --output-on-failure -j4
```

The measured JDK Linux header selects its scalar layout using the target
compiler's `_LP64`; it does not load the host JVM. Adjust explicit paths for
the local installation. These flags instrument the C core and providers with
UBSan, while leaving the combined ASan/UBSan option off. All 13 selected JNI
fixtures passed under GCC 13.3/QEMU 8.2.2 with their original deadlines. This
is ARM64 Linux C execution evidence, separate from Android ARM64 compilation,
ART, physical hardware, ASan/LSan and hardware custody. Continue to run the
normal host safety gates and Android acceptance separately; the broader QEMU
native suite has recorded timeout and sanitizer limitations in PROGRESS.

### Additional uninitialized-read checks

Clang MemorySanitizer supplements the normal ASan/UBSan/LSan gates on Linux
x86_64. Use a separate build so the core, providers and native fixtures all
receive the same instrumentation; OpenSSL/libsodium differential tests and the
real JVM are excluded from this profile. From this directory:

```sh
cmake -S native -B native/build/msan -DCMAKE_C_COMPILER=clang-20 \
  -DCMAKE_BUILD_TYPE=Debug '-DCMAKE_C_FLAGS_DEBUG=-O1 -g' \
  '-DCMAKE_C_FLAGS=-fsanitize=memory -fsanitize-memory-track-origins=2 -fno-omit-frame-pointer -fPIE' \
  '-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=memory -fsanitize-memory-track-origins=2 -pie' \
  -DZCL_SANITIZE=OFF -DZCL_FUZZ=OFF -DZCL_JNI=OFF \
  -DZCL_ORACLE=OFF -DZCL_TLS_REVIEW=OFF \
  '-DZCL_TEST_JNI_INCLUDE_DIRS=/usr/lib/jvm/java-17-openjdk-amd64/include;/usr/lib/jvm/java-17-openjdk-amd64/include/linux'
cmake --build native/build/msan -j8
ctest --test-dir native/build/msan --no-tests=error --output-on-failure -j4 \
  -E '^wallet_(fuzz_profile|test_deadline_contract|test_deadlines|tls_quarantine)$'
```

The four excluded entries are script/build contracts exercised by the normal
safety gate; this run selects native executables. Keep existing test deadlines
and report sanitizer startup failures separately from wallet findings. The
measured Clang 20.1.2 run passed after initialized startup controls and a
deliberate uninitialized heap-read control confirmed the runtime. Exact scope
and evidence are recorded in PROGRESS. This does not provide Android runtime,
race, address-bounds, leak or complete uninitialized-path coverage.

### Unsigned release reproduction

Android native compilation maps the checkout root to `.` in debug information
and file macros, including provider sources. This preserves content-derived ELF
build IDs while removing checkout-location differences from their inputs.
Two builds of the same source with the same installed toolchain, dependencies
and build settings should produce identical unsigned release APKs:

```sh
./gradlew --offline :android-app:assembleRelease
sha256sum android-app/build/outputs/apk/release/android-app-release-unsigned.apk
```

Build each copy from its own source directory without copying `build/`, `.cxx/`
or project `.gradle/` outputs. Compare the complete APK bytes with `cmp`, not
only extracted code or a rewritten ZIP. A local source archive in an isolated
cache directory is sufficient to check checkout-path independence. Preserve the
source commit, toolchain versions, both hashes and any differing artifacts.
The measured acceptance uses two paths on one Linux host; different hosts,
toolchain versions and signed APK reproduction remain unqualified. An equal
hash establishes byte identity, not wallet or hardware-custody safety.

The C core currently covers checked amounts/addresses/payment URIs, English
BIP39 recovery, BIP32/BIP44 Zclassic receiving keys, authenticated-record
serialization, private storage that refuses overwrite, backup confirmation,
custody-policy validation and public receiving QR generation. The launcher now
has create/restore/backup-confirmation/lock/unlock/receive screens, with a thin
Android Keystore and authentication adapter. Successful custody workflows still
require device qualification; the development emulator reports software-only
key protection and the app refuses setup on it. This is not a production wallet.
The [record format](docs/WALLET_RECORD.md) explains authentication and recovery.
An emulator test exercises C storage and GCM with public fixtures; it does not
qualify physical hardware custody.

QR encoding is C, with fixed buffers and a pinned provider. Android only draws
the public modules. ZXing is a test oracle and is not an app runtime dependency.
Balance is explicitly unavailable until a qualified network source is connected.
Bounded C Electrum request/reply parsing, signed balance arithmetic and pinned
genesis-header fixtures are present; [sync scope](docs/READ_ONLY_SYNC.md)
records C/JNI state, bounded foreground display/timer lifetimes and the remaining
TLS, endpoint/privacy and real-network integration work. Public fixtures qualify
unverified/stale display and Activity recreation without opening a wallet.
The [host TLS candidate is quarantined](docs/TLS_REVIEW.md) after a reproducible
certificate-parser sanitizer finding; Android builds exclude its network entry
points and provider translation units while that review remains open. Normal
host builds also exclude TLS; an explicit host review option cannot be combined
with JNI/Android. The independent C sync state now checks request order, identity
before address disclosure and final-tip consistency using offline fixtures.
The C QR decoder has bounded input/work and strict request validation. The
camera screen uses a bounded grayscale preview, an isolated decoder service,
and a public request review; see [scanner scope](docs/SCANNING_QR.md). Actual
camera capture/lifecycle passes on the emulator; physical QR interoperability,
hardware custody, sync, sending, shielded support and mobile validation remain
unqualified or unfinished. See [the next milestone](docs/NEXT_MILESTONE.md).
The separate [scanner UI fixture](scanner-ui-tests/README.md) exercises the
actual permission dialog against a normal locally signed minified APK, with
its own test runtime and no production keep rules.
For new Linux emulator launches, use the qualified
[ADB reaping workaround](docs/EMULATOR_REAPING.md). It completes the SDK's
abandoned timeout waits and refuses unreviewed SDK bytes. Existing emulator
profiles remain intact; host process fixtures and Android behavior are checked
separately.
The offline [transparent transaction codec](docs/TRANSACTIONS.md) now has
bounded C parsing/serialization and independent SHA256d fixtures; it grants no
funding, signing or broadcast authority.

Check captured `am instrument -w -r` output with the expected number of tests:

```sh
bash tools/check-instrumentation-result.sh <expected-test-count> <captured-log>
./gradlew --offline :android-app:checkInstrumentationResults
```

AndroidJUnitRunner can print `OK` while tests were skipped. The result checker
requires every expected test to start and pass, matching identities and sequence,
followed by the matching summary and terminal result. Skips, partial runs and
inconsistent evidence fail. The Gradle task qualifies this checker against local
fixtures and is included in `check`; it does not run device tests. Keep skipped
capability evidence separately and rerun explicitly selected supported tests
with their own expected count. Passing log structure does not authenticate the
device, APK, test selection or hardware custody outcome.

For C safety checks on a Linux development host with Clang 20 and GCC:

```sh
bash tools/check-c-safety.sh
```

This runs source/provider analysis, the Clang sanitizer suite and a separate
optimized GCC ASan/UBSan suite. The GCC profile uses Debug with `-O2 -g`, keeping
assertions enabled; provider-specific `-Os` remains in effect. It retains the
4096-byte fixture frame limits. On glibc, storage fault injection covers the
fortified read entry points as well as ordinary reads, preserving bounds traps.
For the optimized profile with optional OpenSSL/libsodium host oracles:

```sh
cmake -S native -B native/build/gcc-optimized-safety -DCMAKE_C_COMPILER=gcc \
  -DCMAKE_BUILD_TYPE=Debug '-DCMAKE_C_FLAGS_DEBUG=-O2 -g' \
  -DZCL_SANITIZE=ON -DZCL_ORACLE=ON -DZCL_FUZZ=OFF \
  -DZCL_TLS_REVIEW=OFF -DZCL_JNI=OFF
cmake --build native/build/gcc-optimized-safety -j4
ctest --test-dir native/build/gcc-optimized-safety --output-on-failure
```

Every registered host test has an explicit execution deadline. Tests without a
specialized limit receive 60 seconds; existing per-test limits are preserved.
The registered `wallet_test_deadlines` check inspects CTest's generated registry,
and `wallet_test_deadline_contract` rejects empty/missing/zero/negative deadline
fixtures and verifies actual one-second termination of a stalled test. These
limits bound test execution, not Android provider or filesystem latency.
The partial-suffix and corruption recovery matrices run as separate groups;
each retains all cases and the original 30-second deadline.

Host Clang sanitizer builds also check unsigned integer overflow and implicit
integer truncation/sign changes in authored native C, including JNI and fault-test
copies. Provider code keeps ASan/UBSan; its modular arithmetic is outside these
additional checks. GCC retains the ASan/UBSan profile. Three isolated compiler
probes verify that the extra checks stop faults ordinary UBSan permits, and the
fuzz-profile gate verifies the actual authored compile commands. These checks
do not replace explicit bounds/conversion review or instrument Android releases.

LeakSanitizer needs a host that permits its process inspection. Do not disable
it to call a restricted sandbox run successful. The manual pre-commit review is
[C_SAFETY_REVIEW.md](docs/C_SAFETY_REVIEW.md).

The registered JNI sync-owner race fixture also supports a separate host Clang
ThreadSanitizer build. Keep it separate from ASan/UBSan and Android releases:

```sh
cmake -S native -B native/build/thread-safety -DCMAKE_C_COMPILER=clang-20 \
  -DCMAKE_BUILD_TYPE=Debug -DZCL_SANITIZE=OFF -DZCL_TLS_REVIEW=OFF \
  -DZCL_JNI=OFF -DZCL_FUZZ=OFF -DZCL_ORACLE=OFF \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  '-DCMAKE_C_FLAGS=-fsanitize=thread -fno-sanitize-recover=all -fno-omit-frame-pointer' \
  '-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=thread'
cmake --build native/build/thread-safety --target jni_sync_race_tests -j4
TSAN_OPTIONS=halt_on_error=1:report_bugs=1:exitcode=66 \
  ctest --test-dir native/build/thread-safety -R '^wallet_jni_sync_races$' --output-on-failure
```

Two native callers query an owner during closure/replacement, then require
retired callbacks to refuse even when the replacement has the same attempt
token. The fake VM uses independent thread-local result buffers. This tests the
native registry, not a real JVM or every possible thread schedule. The measured
detector rejects an unlocked-registry mutation; see the review and work log.

## Ordered milestones

1. Exact money/address/QR parsing; on-device create/restore; authenticated
   encrypted key storage; receiving address and read-only balance sync.
2. Transparent send construction and signing checked against a pinned
   `zclassicd` reference using public test vectors and isolated fixtures.
3. Shielded transactions only after Zclassic-specific serialization, branch
   IDs, proof parameters, witness/anchor handling and recovery are verified.
4. Bounded mobile chain/header validation with explicit trust guarantees.
5. Separately keyed encrypted messaging module design, off-chain and isolated
   from wallet authority.

See [security design](docs/SECURITY.md) and [work log](docs/PROGRESS.md).
