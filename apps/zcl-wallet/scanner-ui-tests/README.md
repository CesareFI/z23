# Public scanner UI fixture

This is a test-only instrumentation APK with its own Kotlin and Android test
runtime. It links neither the wallet core nor the app. It can drive the normal
minified release through Android framework APIs and public view identifiers,
without adding production keep rules or changing the release's R8 mapping.
Its merged manifest has no activity, service, provider or requested permission;
only the instrumentation entry targets `org.zclassic.wallet.dev`.

Build from the wallet directory:

```sh
./gradlew :scanner-ui-tests:assembleDebug :scanner-ui-tests:check
```

Use a separately created disposable emulator. The fixture requires `ranchu`,
the development target package, denied CAMERA permission and no `wallet-v1`
directory. It opens only the scanner, never the wallet setup Activity. Do not
clear or substitute a wallet directory to satisfy this guard. Keep the emulator
awake and unlocked, and resolve any system error dialog before starting.

Install a locally development-signed release APK and the fixture APK sequentially
with the same development certificate. Installing the fixture requires `adb
install -t`; its manifest always marks it test-only. No production signing key
is needed. On that disposable profile, prepare CAMERA as denied with permission
decision flags cleared, so Android can show the real dialog again.

Select the explicit fixture:

```sh
adb -s "$fixture_serial" shell am instrument -w \
  -e scannerPermissionFixture yes -e requireMinified yes \
  -e class org.zclassic.wallet.ScannerPermissionInstrumentedTest \
  org.zclassic.wallet.scannerfixture/androidx.test.runner.AndroidJUnitRunner
```

`requireMinified` checks that the target is not debuggable. The test waits for
Android UI idleness, injects actual touches, denies the real permission dialog,
and requires its explanation and selected network on return. It also requires
no preview, decoded address, camera worker or wallet directory. A skipped test
or a run blocked by a system dialog is not acceptance. Without the camera opt-in
below, this qualifies denial handling only. It does not qualify hardware custody
or production signing.

To continue the same test through a new permission request, foreground camera
grant and exact review, add `-e qrCameraFixture yes`. This additionally requires
the [documented public PNG camera scene](../docs/SCANNING_QR.md) on the disposable
emulator. It must observe the fixed address, 1.25 amount and CameraFixture label,
review notice, Scan again control, and released camera/preview. The test starts
with denied permission; reset only that disposable profile's permission state
before each complete run. Physical optics and hardware custody remain separate.

Touches use the [AOSP UIAutomator 100 ms press duration](https://android.googlesource.com/platform/frameworks/base/+/4becfb4/cmds/uiautomator/library/core-src/com/android/uiautomator/core/InteractionController.java),
with UI idleness and freshly queried bounds before each gesture. The fixture
does not disable Android's touch filtering or replace the permission dialog
with a direct package-manager grant.
