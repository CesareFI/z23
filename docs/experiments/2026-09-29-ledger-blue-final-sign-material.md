<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Bind final Blue approval to the verified signing material

Date: 2026-09-29T08:38:20-04:00 (2026-09-29T12:38:20Z).
Host: AMD Ryzen 7 PRO 8840U. Compilers: Clang 22.1.6, C23, and
arm-none-eabi-gcc 16.2.0.

## Failure boundary

Wallet 0.3.44 copied the drawn totals, path label, branch, lock time, and
expiry height into its final-screen snapshot. It hashed the verified input
digest records only when SIGN ZCL was tapped. A change to an input digest
after the final screen was drawn could therefore become the material signed
under that unchanged screen. The full unsigned-wire commitment was also
absent from the post-approval signing record check.

Wallet 0.3.45 hashes all fixed input digest records together with the
device-computed full unsigned-wire commitment before drawing the final page.
The hash occupies unused address-line storage on that page; no new static
buffer is allocated. The approval callback rehashes and compares it. The
post-approval record check uses the same two-part binding, including inside
the signer callback. A changed input record or commitment aborts the review
before a signature can be returned.

## Evidence

The BOLOS-shim UI test changes one input digest and, separately, the
full-wire commitment while the final page is visible. Both cases reject
the SIGN ZCL callback, erase the review, and leave the signer uncalled. A
third case changes the commitment inside the signer callback; the pending
signature reply is erased. Existing two-input signing and interruption tests
still pass. The full Release suite and the sanitized Debug suite each passed
63/63 tests. Debug used `ASAN_OPTIONS=detect_leaks=0` because LeakSanitizer
cannot initialize in this execution environment.

The integrated app-loop test also drives the exact synthetic review through
USB commands and Blue-format finger events. After the final page appears,
it changes the stored input digest or commitment, taps SIGN ZCL, and checks
that the device loop calls no signer, erases its review, and rejects a later
signing APDU. Both focused Release and sanitized Debug runs passed. This is
BOLOS-shim evidence; it does not substitute for physical touchscreen and USB
verification.

Two clean builds of Wallet 0.3.45 with separate copies of Blue SDK
`3c710b4c62ad847599a2deb0932a50dd1ae4bdff` and identical reviewed
C23 patches produced the same 54,016-byte `.text` SHA-256
`5b7bb5a205ca4cc281e6bbdc4582b1f558babd871c058ed6ede7ec0a2ba59b93`.
Both Intel HEX files have SHA-256
`4ef860dc25be9924edffd0854d4ddbadcbf3864bde13fc3b5c5a398b23d12d08`.
The image has zero `.data` and 5,120 bytes of `.bss`, including its 2 KiB
reserved stack. The build gate reports a 920-byte modeled approval path and
1,096-byte modeled signing derivation path, plus a 512-byte reserve for
unmodeled frames. BOLOS runtime frames remain outside this measurement.

The offline installer recognizes the exact image as Wallet 0.3.45 and
reports `installation blocked pending physical validation`. The image has
not been installed or opened on a physical Blue. This change binds stored
signing material to the final screen; it does not add chain verification,
shielded signing, or a verified recipient policy.

The installer previously accepted only five-character app versions when
encoding the BOLOS install parameters. The 0.3.45 build has a six-character
version, so it would have failed before installation even after its policy
block was lifted. The C23 encoder now accepts bounded numeric versions up
to seven characters, checks the complete parameter size before writing,
and matches the exact 0.3.45 parameter bytes in a host test. The installer
policy remains blocked; encoding success does not authorize installation.
