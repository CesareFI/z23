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
No image, payload, key or recovery phrase is logged, saved or transmitted.

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

A camera permission/lifecycle adapter and visible review screen are still
required. They must be separate from custody/setup screens, display the selected
network and complete request, retain no background frames, bound pending work,
and never authorize a send. Device camera interoperability, permission denial,
background/cancellation and process-recreation acceptance remain pending.
