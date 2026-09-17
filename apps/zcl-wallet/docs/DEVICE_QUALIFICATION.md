# Device qualification build boundary

Positive physical-device custody remains unproven. The qualification build
provides an isolated application identity for attended public-vector acceptance.
The restore/repeated-unlock fixture is implemented but has not run successfully
on physical hardware. It does not relax wallet policy.
The existing `WalletFlowInstrumentedTest` remains emulator-only and refuses the
qualification package before taking ownership of files or keys.

Build from the wallet directory:

```sh
./gradlew --offline -PwalletQualification=public-custody \
  :android-app:checkQualificationIsolation
```

Only this exact property value is accepted. Omitting the property builds the
normal wallet. Qualification mode disables the wallet release variant; requesting
`:android-app:assembleRelease` in that mode fails. It produces:

- Application ID `org.zclassic.wallet.dev.qualification`, visibly labeled
  `Zclassic public custody fixture`.
- Instrumentation ID `org.zclassic.wallet.dev.qualification.test`, targeting
  only that application.
- Explicit `testOnly=true` in both APKs, requiring `adb install -t`.
- Outputs under `.cache/android-wallet/qualification-build/android-app/`,
  separate from the normal `android-app/build/` outputs.

The APK paths below that output directory are
`outputs/apk/debug/android-app-debug.apk` and
`outputs/apk/androidTest/debug/android-app-debug-androidTest.apk`.
Build different profiles serially in one checkout: Gradle and native metadata
still share the checkout. No signing credentials, dependencies, C/JNI sources,
Keystore alias, wallet path, authentication predicate or TLS setting change.
Android supplies a different application UID, and thus a separate private
filesystem and Keystore namespace; shared UIDs are rejected by the APK gate.

`checkQualificationIsolation` inspects the actual merged APK manifests for exact
package/instrumentation runner and target identities, explicit test/debug flags,
application backup refusal and absence of shared UID. A control and twenty metadata
mutations qualify this narrow gate. The normal fixture checker independently
rejects qualification package IDs or flags in normal artifacts, including an
incorrect instrumentation target. These checks establish artifact identity,
not physical hardware, authenticated encryption or complete process confinement.

On the API35 emulator, installation without test opt-in is refused; explicit
test installation has a UID distinct from the normal development wallet. The
public software-GCM custody fixtures execute in that separate namespace, and
the original emulator-only wallet fixture refuses its package. Only packages
created by that invocation are removed. This exercises installation isolation
without generating a Keystore wallet or authenticating a physical device.

## Attended public-vector restore

Use only an explicitly selected fresh physical test device/profile. Install the
two qualification APKs with `adb -s <device> install -t <apk>`; do not use `-r`
to replace an existing installation. Obtain its exact assigned UID with
`adb -s <device> shell pm list packages -U org.zclassic.wallet.dev.qualification`
and select the application row, not the instrumentation row. Then run:

```sh
adb -s <device> shell am instrument -w -r \
  -e class org.zclassic.wallet.AttendedCustodyInstrumentedTest \
  -e custodyHardware restore-public-vector \
  -e qualificationUid <application-uid> \
  org.zclassic.wallet.dev.qualification.test/androidx.test.runner.AndroidJUnitRunner
```

Preserve the command output in an ignored local log (use `tee` if the owner
needs stage output during authentication), and require
`bash tools/check-instrumentation-result.sh 1 <captured-log>` before recording a
passing run. An `OK (1 test)` summary alone also appears for a skipped fixture.

The fixture requires the exact test-only/debuggable package, matching process
and explicitly supplied application UID, and non-emulator Build metadata before
inspecting private storage or the Keystore. Build fields are a known-emulator
refusal, not hardware attestation. It then requires an absent `wallet-v1` path
under a real parent directory and an absent wrapping-key alias. Existing files,
empty directories, symlinks or inspection errors refuse; nothing is deleted.
No opt-in argument skips the test; malformed supplied consent fails. Neither
a skip nor an admission refusal establishes positive custody acceptance.

The owner authenticates through three real system prompts: restore and two
subsequent independent unlocks. Each awaited UI stage has a 75-second bound.
The fixture reports fixed stage names, injects no credentials, reads no system
credential views and captures no screens. It restores only the published
128-zero-bit BIP39 vector (`abandon` eleven times, then `about`), with empty
passphrase on testnet. The expected `m/44'/1'/0'/0/0` address is
`tmF1xjfhsSzhy55dmhorzTnKjtHhZmPKzts`, independently checked using the existing
OpenSSL oracle. This public fixture must never receive funds.

After each successful operation it verifies the exact address, secure-window
flag, committed unchanged ciphertext and absent historical change initialization.
A separate unauthenticated decrypt must fail specifically for authentication,
and the real provider key must pass the existing custody policy. An unrelated
provider error is not accepted as per-use authentication evidence.

The resulting public wallet and key are deliberately retained, including after
failure. A rerun refuses existing state. Inspect and preserve the evidence before
any owner-directed removal of this qualification installation. The fixture has
no key/file cleanup or normal-wallet namespace access.

On API30/35/36, all six synthetic admission tests and the unauthenticated-key
policy refusal pass. The separate per-use provider test skips because those
devices have no configured test screen lock; it contributes no passing evidence.
On API35,
the attended fixture refuses the normal package, emulator, missing UID and
malformed consent before wallet access; its no-consent run skips. No physical
device was attached. Creation with written-backup confirmation, actual hardware
restore/repeated unlock, cancellation, key invalidation, process death,
interrupted-record recovery and minified hardware execution remain open.
See [the ordered acceptance contract](NEXT_MILESTONE.md) and
[custody boundaries](KEYSTORE_PLATFORM.md).
