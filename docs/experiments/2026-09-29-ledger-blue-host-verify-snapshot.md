<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Ledger Blue signing verification snapshot

## Finding

The host's authenticated transparent assembler copied the unsigned wire,
reviewed hash, signature records, derivation paths, public-key hashes, and
expected ZIP-243 digests before invoking callbacks. It assembled from that
copy and rejected caller mutations afterward. Its verification step still
read the caller-owned signature and digest arrays. A callback that changed
the caller digest during verification changed the bytes supplied to the
signature verifier, despite the earlier snapshot.

The regression injected a digest mutation from the verifier callback and
checked which bytes the verifier received. It failed before the change:
the verifier observed the modified caller digest. This is a verification
input ambiguity at the final signed-wire boundary.

## Change and observed result

The authenticated assembler now verifies the copied signature records,
digests, paths, and public-key hashes. It still compares all caller inputs
with the copy after callbacks. The same injected mutation therefore leaves
the verifier's digest equal to the reviewed snapshot, rejects the changed
caller storage, sets output length to zero, and erases the full output
capacity. The complete review-to-signed-wire fixture and two-input path
remain passing in Release and sanitized Debug.

This is a host signing-boundary change; no Blue app image or device key path
changed. It does not establish physical Ledger Blue behavior or shielded
payment signing.

On 2026-09-29T09:39:27Z, Clang 22.1.6 on an AMD Ryzen 7 PRO 8840U built
the host targets. The serial Release and sanitized Debug suites each passed
62/62 tests. The unchanged consensus-core seal matched 554 files and 80
sections; all 33 fast lint gates and the inline-path, document-claim, and
Markdown-link gates passed.
