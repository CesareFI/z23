# Android wallet BLAKE2b reference provider

Source: official [BLAKE2/BLAKE2](https://github.com/BLAKE2/BLAKE2) commit
`ed1974ea83433eba7b2d95c5dcd9ac33cb847913`, `ref/blake2b-ref.c`,
`ref/blake2.h`, `ref/blake2-impl.h` and complete upstream `COPYING`.
The headers and license are unchanged. `SHA256SUMS` pins the integrated bytes.

The sole local source change adds `memset(P, 0, sizeof(P))` immediately after
both `blake2b_param P[1]` declarations. The full parameter object's initialized
representation is explicit before its byte-oriented `load64` reads. This
resolves the original Clang analyzer finding without a suppression or changed
threshold; the diagnostic did not establish a runtime defect. Original source
SHA256: `e2bf9872a8f0a51711b765936d420a7f8c34797db4d938948c24f2ddbc1dc588`.

Only the wallet's internal bounded public-data, unkeyed sequential BLAKE2b-256
wrapper is qualified for use. Salt is zero; personalization is exactly 16 bytes;
input is at most 4096 bytes. Generic upstream keyed/tree/parallel interfaces
are not wallet APIs. No provider claim authorizes secret-key hashing, signing,
or consensus changes. The separate TLS quarantine remains in force.

Review and reproducible qualification: [BLAKE2_REVIEW.md](../../apps/zcl-wallet/docs/BLAKE2_REVIEW.md).
