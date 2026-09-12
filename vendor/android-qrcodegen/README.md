# Pinned C QR encoder

`qrcodegen.c` and `qrcodegen.h` are unchanged files from Project Nayuki's
QR-Code-generator v1.8.0, commit `720f62bddb7226106071d4728c292cb1df519ceb`.
Source: https://github.com/nayuki/QR-Code-generator/tree/720f62bddb7226106071d4728c292cb1df519ceb/c

Both files retain their complete MIT license. `SHA256SUMS` pins the downloaded
bytes; the release tag is unsigned, so these hashes identify the reviewed
source and do not establish a publisher signature.

Only the portable C implementation is included. The wallet calls the explicit
length binary encoder with two fixed 138-byte buffers, QR versions 1..4 and
quartile error correction. It first validates an exact 35-byte Zclassic public
address. The provider allocates no heap memory, retains no state and performs
no I/O. Its broader text/segment APIs are not exposed through JNI.

The upstream C test program (kept in the development review directory, not
linked into the app) passed all 521 cases with Clang 20 ASan/UBSan/LSan on
2026-09-12. Wallet wrapper and independent decoder tests cover the narrower
application interface separately.
