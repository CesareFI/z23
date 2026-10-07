# Receiving-address QR

C validates the exact 35-byte transparent address and selected Zclassic network
before encoding. The public API returns 0/1 modules with an included four-module
white border. No secret, recovery phrase, arbitrary payload, URI or network
service enters this interface. The Android view draws those modules at integer
pixel sizes on white; address text remains available if generation fails.

The pinned [Nayuki C provider](../../../vendor/android-qrcodegen/README.md) uses
quartile error correction, automatic mask selection and versions 1..4. The
current address size produces a version-4 symbol (33 modules), or 41 modules
including the border. Provider buffers are fixed at 138 bytes; expanded output
is bounded by 1681 bytes, and the JNI record adds one width byte. No native heap
allocation, callback, I/O or retained pointer is involved.

After every fallible check succeeds, C clears and fills only the validated
caller-owned output span. It does not stage a second expanded image on the
native stack. Failures leave the module buffer and side unchanged; capacity
beyond the returned square is never written.

The C tests verify both networks, public P2PKH/P2SH fixtures, malformed addresses,
every insufficient output capacity, unchanged outputs on failure, canaries and
the quiet border. Sanitizers and static analyzers run through the normal safety
script. A bounded fuzzer also derives valid public addresses from arbitrary
20-byte hashes to reach the encoder instead of exercising only parser rejection.

The independent Java oracle is test-only ZXing 3.5.4. It decodes the C modules
directly, then detects and decodes 48 public P2PKH images at scales 2, 5 and 9
in two orientations (288 rendered cases), plus a P2SH fixture. Every image must
produce exactly one successfully decoded QR containing the exact address.

Its single-candidate image detector rejects a valid synthetic fixture (mainnet
fixture 16 at scale 5 after rotation). Direct module decoding and the
multi-candidate detector recover the exact address. The pinned regression
retains both observations. The [single reader](https://github.com/zxing/zxing/blob/zxing-3.5.4/core/src/main/java/com/google/zxing/qrcode/QRCodeReader.java)
decodes one detected pattern set; the
[multi reader](https://github.com/zxing/zxing/blob/zxing-3.5.4/core/src/main/java/com/google/zxing/multi/qrcode/QRCodeMultiReader.java)
tries additional candidate sets. A successful alternative candidate still must
pass the normal QR checksum and decoding checks; no invalid decode is accepted.

These tests establish standard symbol encoding and synthetic interoperability.
They do not guarantee every external scanner's detector succeeds for every
viewing condition. Real-device rendering and camera interoperability remain
required. The separate C camera decoder and scanning UI have their own
[bounded capture and public-request acceptance](SCANNING_QR.md).

The Android instrumentation adds Canvas rendering at square/portrait/landscape
sizes and independent decoding of the resulting in-memory pixels. It uses only
public fixtures, never an activity screenshot or file, and separately checks
that an undersized view does not display a cropped QR.

The host JNI fixture/fuzzer now checks the exact width/module projection for
the original public P2PKH/P2SH address vectors on both networks. It injects
partial reads/writes, NULL allocation with and without an exception, a returned
reference with a pending exception, and an already-pending call. Input snapshots,
fixed result canaries and exact VM-call counts check the adapter boundary; the
C encoder supplies its projection reference, not an independent QR oracle.
The Android Canvas oracle also includes both P2SH network vectors at all three
aspect ratios, retaining the original P2PKH and too-small-view checks.
