# Device qualification build boundary

Positive physical-device custody remains unproven. The qualification build
provides an isolated application identity for future public-vector acceptance;
it does not implement that hardware acceptance fixture or relax wallet policy.
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

Next acceptance work must add a separately reviewed fixture that requires an
explicitly selected fresh device/profile, refuses existing wallet/key material
before taking ownership, uses only published unfunded recovery data, and lets
the operator authenticate through the real system prompt. It must never read,
inject or record a device PIN, capture a secret screen, replace an existing key,
or weaken the C custody predicate. Restore, repeated per-use unlock, cancellation,
key invalidation and interrupted-record recovery need actual hardware evidence.
See [the ordered acceptance contract](NEXT_MILESTONE.md) and
[custody boundaries](KEYSTORE_PLATFORM.md).
