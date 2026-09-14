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
The offline [transparent transaction codec](docs/TRANSACTIONS.md) now has
bounded C parsing/serialization and independent SHA256d fixtures; it grants no
funding, signing or broadcast authority.

For C safety checks on a Linux development host with Clang 20 and GCC:

```sh
bash tools/check-c-safety.sh
```

LeakSanitizer needs a host that permits its process inspection. Do not disable
it to call a restricted sandbox run successful. The manual pre-commit review is
[C_SAFETY_REVIEW.md](docs/C_SAFETY_REVIEW.md).

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
