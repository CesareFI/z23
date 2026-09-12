# Bounded Android quirc provider

Upstream: https://github.com/dlbeer/quirc

Pinned commit: `927d680904dc95fdff4cd9d022eb374b438ff8f2` (2025-05-20).
Only the four library C files, two headers and ISC license are included; no
demo, image-file parser, camera backend or upstream build script is executed.
`UPSTREAM_SHA256SUMS` identifies original files from that commit.
`SHA256SUMS` identifies the locally hardened files that are actually compiled.
`LOCAL_HARDENING.patch` records the differences. Hashes identify bytes and do
not constitute a signature, audit or safety proof.

This copy has deliberate local changes. It is not pristine upstream:

- Reject dimensions outside 1..1024 before allocation, check size arithmetic
  before multiplication, and avoid the first-resize NULL-source zero-byte copy.
- Reject nonfinite/excessive projected coordinates before integer conversion;
  callers handle failure. Widen line-intersection and alignment-area arithmetic.
- Validate grid versions and extraction indices before array-address formation;
  reject alignment estimates outside the image.
- Bound grid attempts (128), alignment search (twice the image pixel count),
  and fitness cells (131072) per frame. Report budget exhaustion so the C wallet
  wrapper rejects the whole frame instead of accepting a partial result.
- Refuse unknown QR payload modes instead of silently truncating a payload.
- Clear owned pixels, geometry and context with volatile stores before freeing
  or replacing them, plus decoded/mirrored stack buffers before returning.
  The wallet wrapper also clears its decoded workspace.

Review references: upstream [issue 157](https://github.com/dlbeer/quirc/issues/157),
[PR 158](https://github.com/dlbeer/quirc/pull/158) and
[PR 159](https://github.com/dlbeer/quirc/pull/159). These are reports/proposals,
not merged upstream fixes or independent verification. Local UBSan probes
reproduced the NULL-source copy and infinity-to-int conversion against the
pinned original source. The local fixes go beyond those proposals, including
checked allocation bounds, propagation of projection failure and total work
budgets. They require their own regression and fuzz evidence.

The provider retains upstream's internal complexity. Authored wallet/JNI code
has a separate complexity-at-most-10 gate; all provider code participates in
ASan/UBSan/LSan and the image fuzzer. Existing unused-parameter and comparisons
of bounded nonnegative signed/unsigned indices are reviewed compiler warnings,
not disabled sanitizer findings. The provider API is hidden in the Android
shared library and reachable only through the checked wallet adapter.
