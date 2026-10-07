# Wallet host-review provider patches

This selected-source provider is Mbed TLS 3.6.7 with the narrowly scoped local
patches below. It is **not** an unmodified upstream release. Normal wallet builds
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

`patches/mpi-temporary-lifetimes.patch` removes three overwritten initial values
in `mbedtls_mpi_core_lt_ct` and `mbedtls_mpi_core_random`. The comparison's
iteration-local temporary is initialized at its first use. The sampling
predicates are assigned before every loop-condition read; error paths leave
through cleanup first. Used constant-time operations, input bounds, retry limits,
RNG errors and cleanup are unchanged. No cryptographic algorithm is replaced.

Original `library/bignum_core.c` SHA-256:
`0881aacce52499184aa33641c24ca6075bf533b31f61c5358923a655490bcfa6`.
Patched file:
`0008def1b3a54a1e5fb1d21237f3de4eef20afdd3038d19b67ff8a23ff492db7`.
Patch:
`79da8c2d340ea89f1482e28f6a203179c63c719cbd37bce728cf1cb843ccf8e2`.
The same reverse-application procedure reconstructs the original source.

Public fixtures compare 65,824 bounded limb pairs with an independent oracle
and exercise 14 deterministic sampling/boundary/error cases. Four mutations
are detected: reversed comparison, ignored lower bound, ignored upper bound
and changed retry cap. Clang whole-unit analysis no longer reports the three
dead stores. Function complexity remains 2 and 4 respectively.

Optimized x86-64 Clang/GCC inspection finds removal of unused volatile-zero
loads and independent register scheduling, with the used masks, comparisons,
loop bounds and sampling operations retained. The objects are not byte-identical.
This evidence does not prove constant-time execution on every platform, qualify
Android transport, or resolve findings elsewhere in the TLS provider.

`patches/rsa-public-import-status.patch` removes two unused assignments in
`mbedtls_rsa_parse_pubkey`. Both imports still run, and each failure still maps
to `MBEDTLS_ERR_RSA_BAD_INPUT_DATA`. ASN1 errors, cursor bounds and public-key
checks retain their existing behavior. Function complexity remains 10.

Original `library/rsa.c` SHA-256:
`4856d43dc23d7ab702b003112f9d624b8b0fba303f311f0e852f2100e5ad8e4b`.
Patched file:
`f8feabd1d2be6a821e5974baa4fd0d8169f4088aa20979015ff9a280bd3fb3b5`.
Patch:
`b52949a9f696c597f78cc90b0dd5ad0ad948430381af88c84abeb5ca75a2f73d`.
Reverse application reconstructs the original source. Optimized x86-64 Clang
and GCC `.text` sections match their respective originals byte for byte.

The Linux host parser fixture checks every truncation of a fixed public DER
input using exact-size allocations, malformed tags/lengths, invalid components,
trailing bytes, both import-allocation failures and complete context cleanup.
Its small structural modulus is not a trusted key and remains below the
transport's security minimum. Mutations of either import-error mapping and
either trailing-length guard are detected. The remaining RSA uninitialized
buffer analysis finding is not waived; this patch does not enable TLS.
