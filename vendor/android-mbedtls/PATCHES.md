# Wallet host-review provider patches

This selected-source provider is Mbed TLS 3.6.7 with the narrowly scoped local
patch below. It is **not** an unmodified upstream release. Normal wallet builds
use only the existing hash subset; Android/JNI TLS remains disabled.

Upstream release: https://github.com/Mbed-TLS/mbedtls/releases/tag/mbedtls-3.6.7
Archive SHA-256:
`a7e8bcbec0e6f761b4af24f25677626b35f762f68eef79c08677a363212d11f6`.
`UPSTREAM_SHA256SUMS` preserves the original selected-file manifest.
`SHA256SUMS` checks the actual patched build inputs; no file was excluded.

`patches/empty-name-comparison.patch` avoids passing a NULL pointer to `memcmp`
for zero-length OID/value buffers in parsed empty X509 Names. Nonempty bytes,
lengths, ASN1 tags, text-case rules and list structure compare as before. This
neither trusts an empty name nor bypasses certificate/hostname verification.

Original `library/x509_crt.c` SHA-256:
`3b484debf0811babaa87450f689e38d7ed60257193c67bbe78f2c89d4226b027`.
Patched file:
`dae6b63ba0b9da87fc42a01590f7ac8e26a33e22c6b902f7c7a32d9d0e1f6d77`.
Patch:
`580ec4b78a0ba9f14ce4c42d9e2cfcfe4299b8f3b5a9448b135622971c066908`.
Reverse application with `git apply --reverse --unidiff-zero` in a disposable
copy reconstructs the original hash. The zero-context format avoids retaining
blank context lines as trailing whitespace in this tracked patch artifact.

The saved certificate-flight regression reproduces both undefined operations
on the original/OID-only versions. Empty/nonempty, typed-value and name-list
fixtures retain the comparison contract. The transport tests retain positive
authenticated exchanges and negative certificate, hostname and allocation
cases. Mutation tests detect removal of either safe comparison and accidental
equality of nonempty unequal bytes. See the wallet's `docs/TLS_REVIEW.md` for
remaining provider findings and qualification limits. This patch does not by
itself qualify the TLS integration for wallet use.
