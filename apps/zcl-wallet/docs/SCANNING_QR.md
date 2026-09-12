# Public receiving-request QR decoding

`zcl_scan_qr` accepts explicit image length, dimensions, row and pixel strides,
and a configured Zclassic network. It copies a bounded luminance image into a
private C decoder, requires exactly one detected QR, checks QR error correction,
and passes its payload to the existing C payment parser. The result is public
receiving-request data, never spending authorization or a secret import.

The scanner supports bare transparent addresses and the implemented `zclassic:`
request fields, with the same address checksum/network, integer amount,
duplicate/unknown field and UTF-8 validation as manually entered requests.
Mirrored symbols get one additional error-correction attempt. Unsupported QR
modes/character sets, invalid payloads, multiple detected symbols and exhausted
work budgets are refused. No failing decode is used as a successful result.

The thin JNI adapter returns the existing bounded payment display record. It
does not pin arrays, hold native handles or retain frames. The caller owns a
stable managed image for the call and must clear it afterward. Native copies,
decoder image/context and decoded workspace are cleared before release. Camera
and runtime copies outside these owners cannot be claimed erased by this API.
No image, payload, key or recovery phrase is logged or persisted. The camera
adapter transfers one bounded frame through local Binder to its isolated
decoder; nothing is transmitted over a network.

## Bounds and ownership

- Each image dimension is 21..1024; pixel stride is 1..4 and row stride at most
  8192 bytes. The input is at most 8 MiB. Exact final-row padding is optional.
  Validation proves the last addressed pixel exists before allocation or read.
- The provider owns at most a 1 MiB grayscale image, fixed context and bounded
  flood-fill workspace. A separate fixed decoded workspace is about 13 KiB.
  JNI may own one additional input snapshot of at most 8 MiB. There is no native
  handle, persistent frame cache, variable-length stack array or recursive fill.
- Per-frame work limits are 128 candidate group attempts, two alignment visits
  per image pixel and 131072 fitness cells (at most 1179648 projection samples).
  Exhaustion rejects the frame; the camera adapter must also bound its queue
  and sampling rate. This establishes operation counts, not a universal device
  latency guarantee.

The [pinned provider](../../../vendor/android-quirc/README.md) includes explicit
local hardening. Original source reproduced two UBSan failures before these
changes. Its upstream version string alone does not identify the reviewed code.

## Verification and remaining acceptance

Native fixtures exercise 16 public addresses over four rotations and pixel
strides 1..4, optional final-row padding, payment metadata, network mismatch,
blank/unsupported payloads and adversarial span bounds. Provider regressions
exercise failed projection, first resize, invalid indices, unsupported mode
and work budgets. Allocation fault injection checks every allocation failure,
zeroed owned allocations before free and exactly-one cleanup. Failures must
leave caller output byte-for-byte intact.

Six JVM/JNI cases use an independent ZXing encoder and verify exact parsed
requests, unchanged input, rotation/mirroring, padded/interleaved planes,
malformed sizes, unsupported requests and multiple symbols. Two Android tests
exercise the real JNI boundary on public synthetic frames. These are synthetic
decoding tests, not camera or complete scanner UX acceptance.

The final source passed 12088 image-fuzzer executions in 301 seconds with
ASan/UBSan/LSan, no finding, and matching final source/provider/binary hashes.
The two Android cases passed on the API-35 development emulator in 6.670 seconds.
The complete C check passes 19 executables, authored GCC/Clang analysis,
provider Clang analysis and authored complexity <=10. All 38 JVM/JNI tests,
debug/instrumentation assembly, unsigned release assembly and Android lint pass.

The evidence above describes the decoder-only checkpoint. The camera adapter
and its additional evidence are described below.

## Camera and isolated decoding

The welcome and receive screens open a private scanner Activity. The selected
network is fixed during capture. Starting the camera requests runtime camera
permission; scanning cannot open a wallet session. Leaving the receive screen
locks that session through the existing lifecycle. Recovery/setup screens do
not expose the scanner. A valid result stops capture and displays its network,
full address, optional amount, label and message. It never authorizes a send.

`CameraCapture` confines Camera2 devices, sessions, ImageReader and Image planes
to one handler thread. ImageReader holds at most two images, and every acquired
image closes in finally. A direct Y-plane pointer is borrowed synchronously by
JNI only while its Image remains open. Closing ImageReader is posted onto that
same handler, so background cleanup cannot invalidate a concurrent C copy.
An outstanding OS camera-open callback keeps its owner alive until its terminal
response; a process-wide token permits at most one pending camera owner. There
is no growing retry/thread queue if a driver does not respond.

C validates the original plane, samples dimensions to at most 384 each, and
produces an exact versioned luminance packet of at most 147461 bytes. The adapter
selects a supported YUV camera size of at most 640*480 pixels and dimensions
240..1024. Preview and decoding use the same sampled grayscale pixels, with the
entire frame visible. Presentation samples at most four frames per second; this
does not claim to limit the camera sensor itself to four frames per second.
One frame may be outstanding. New camera images are dropped/closed while busy.

The non-exported decoder service uses Android `isolatedProcess=true` and has no
app permissions. The client verifies the installed service flags, performs a
Binder handshake and refuses the app's own UID as the decoder. The service
accepts only its owning app UID. Both sides bound pending work; startup is
limited to 15 seconds and replies to 5 seconds. A timeout closes the client and
returns to an explicit retry screen. These deadlines are refusal policy, not
proof of universal decode latency or immediate termination of OS work.

The service's C core validates both the packet and QR payload. It returns at
most 1024 bytes of exact request text. The app's C parser validates that text
again against the selected network before display Strings are created. The
decoder never supplies trusted display fields, key handles or spending actions.
Queued arrays, JNI copies, C decoder work, and preview owners clear on their
respective cleanup paths. Camera-driver, Binder, VM, GPU and runtime copies
cannot all be claimed erased. Frames and requests are not saved in Activity
state, logs, files, telemetry or network requests.

Android contracts: [isolated services](https://developer.android.com/guide/topics/manifest/service-element),
[ImageReader ownership](https://developer.android.com/reference/android/media/ImageReader),
[camera orientation](https://developer.android.com/media/camera/camera2/camera-preview).

## Camera checkpoint evidence — 2026-09-12

- All 20 native executables pass ASan/UBSan/LSan, with authored Clang/GCC and
  provider Clang analysis, strict warnings, and authored complexity <=10.
  New cases check exact pixel sampling, canaries, short planes/capacities,
  canonical packets, network mismatch and exact returned public text.
- All 41 JVM/JNI cases pass with `-Xcheck:jni`. New cases cover direct, read-only
  and sliced ByteBuffers with offsets/limits, non-direct/invalid ranges,
  independently encoded QR requests, wrong networks and malformed IPC text.
- Camera-packet fuzzing completed 12070 executions in 301 seconds without a
  sanitizer finding (263 MiB reported RSS). Source/provider/binary hashes
  rechecked afterward under `native/build/fuzz-scan/camera-final`.
- API-35 emulator: isolated UID plus valid/wrong-network public QR Binder
  round-trip passes (23.136 seconds); denied camera permission produces no
  frame and releases its worker (0.642 seconds); actual camera frames and
  three background/resume cycles pass (149.774 seconds). Every cycle clears
  preview ownership, terminates the camera thread and requires explicit restart.
- Debug/test assembly, unsigned R8 release assembly, debug/release Android lint
  and the repository's 32 lint-fast gates pass. No minified runtime claim.

These are separate isolation, synthetic QR and actual camera lifecycle tests.
Actual camera QR-to-review interoperability, physical-device preview rotation,
runtime permission-dialog interactions, cancellation during pending OS open,
process recreation and oldest/current API coverage remain unqualified.

The `CameraLifecycleInstrumentedTest` methods require different permission
fixtures and must be invoked separately on the dedicated development emulator.
Use `-e cameraFixture yes` and select one method with `-e class`:
`org.zclassic.wallet.CameraLifecycleInstrumentedTest#deniedPermissionClosesCameraWorkerWithoutFrame`
with CAMERA denied, or
`org.zclassic.wallet.CameraLifecycleInstrumentedTest#actualFramesStopOnBackgroundAndRequireExplicitRestart`
with CAMERA granted. The instrumentation component is
`org.zclassic.wallet.dev.test/androidx.test.runner.AndroidJUnitRunner`.
The tests assert permission state; they never revoke permissions or remove any
wallet. Do not run this opted-in fixture on a physical/operator device.

2026-09-13: `recreationClearsCapturedFrameAndRequiresExplicitRestart` passed on
the API-35 emulator in 114.094 seconds. It receives a real camera frame, recreates
the Activity, observes the old preview cleared and camera worker terminated,
requires an explicit restart, receives a new frame through a different preview
owner, then checks final background cleanup. Select this method with CAMERA
granted and the same emulator opt-in. Debug test assembly and Android lint pass.
This is Activity recreation evidence; OS process death, physical-device camera
interoperability and the other outstanding acceptance items remain separate.

The scanner now records network selection immediately, including before the
user starts capture, and saves only that mainnet/testnet preference in Activity
state. The launch Intent remains the initial default. Background/resume and
recreation retain a user's changed selection; frames, decoded requests and
permission/capture continuation remain unsaved and require a new action.
`ScanStateInstrumentedTest` exercises this without camera permission, capture,
wallet access or external traffic. Its selected-network assertion fails against
the preceding implementation; the initial fixture was corrected to inspect the
radio's checked state rather than the click-listener return value.
The corrected test fails on the old selected-network state in 32.265 seconds
and passes on the fixed implementation in 46.999 seconds. Debug/test and unsigned
minified release builds plus both Android lints pass. This is public Activity
state evidence; the separate physical-camera and process-death limits remain.
