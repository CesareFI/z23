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
