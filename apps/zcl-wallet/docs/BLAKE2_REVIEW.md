# BLAKE2 reference candidate: BLOCKED — REQUIRES FURTHER SECURITY REVIEW

This candidate is confined to the repository scratch review area. No provider
source, wrapper, CMake target, JNI entry or signing path has entered wallet
builds. The separate TLS quarantine remains unchanged. Continue independent
wallet work; do not repeatedly retry this blocked provider investigation.

Candidate: official BLAKE2/BLAKE2 commit
`ed1974ea83433eba7b2d95c5dcd9ac33cb847913`, obtained via public HTTPS on2026-09-13.
The official [BLAKE2 site](https://www.blake2.net/) links that source repository.
The downloaded `ref/blake2b-ref.c`, `ref/blake2.h`, `ref/blake2-impl.h`, `COPYING`
and `testvectors/blake2-kat.h` are unchanged. Hashes identify reviewed bytes;
no publisher-signature claim is made.

Pre-integration Clang20 static analysis reported:

```text
blake2-impl.h:59:32: error: The left operand of '<<' is a garbage value
[core.UndefinedBinaryOperatorResult]
```

One bounded diagnostic follow-up with text output traced the path from the
`blake2` alias through unkeyed `blake2b`, `blake2b_init`, `blake2b_init_param`
and `load64` while reading the parameter block. The source assigns parameter
fields and clears reserved/salt/personal storage, so the diagnostic alone does
not establish a runtime uninitialized read. Whether this is a defect or analyzer
model limitation remains unresolved. No suppression, provider patch, changed
threshold or false-positive waiver was applied.

The review script stopped on this first analyzer failure. GCC analysis and
upstream ASan/UBSan/LSan self-test were not reached; no runtime crash, sanitizer
result or cryptographic failure is claimed. Host libsodium1.0.18 is available for
an eventual independent personalized-hash oracle, but no such oracle ran yet.

Evidence is preserved under
`.cache/android-wallet/48h-20260912T235829Z/blake2-review/`: exact source/license/
known-answer vectors, `SHA256SUMS`, original analyzer log, text path diagnostic,
and any generated analyzer artifacts. The parent area retains the public
upstream commit response, `check-blake2-provider.sh`, result1 and
`blake2-review-evidence.tar` with its separate SHA256 manifest. The archive
preserves the initial state without overwriting earlier wallet/TLS evidence.
Archive SHA256:
`0026d784348a7b998c57876839c37c03b3715a1e406b26efdd9980f262a5fad1`.

Primary source SHA256:
`e2bf9872a8f0a51711b765936d420a7f8c34797db4d938948c24f2ddbc1dc588`.
Header SHA256:
`389bc87a83cdd9e25569a294d01a3347970d117237a66eee9df8edd6058736a4`.
Byte-helper SHA256:
`bc0ead7f3259a415325fa40ddebb1876f903d5062d888fc5994e8b2d9e616ec4`.

Further security review must resolve the reported byte-initialization path and
qualify any exact patched or alternative provider with unchanged strict checks,
independent personalized BLAKE2b256 vectors, sanitizer/fuzz evidence and Android
builds before a signature-hash implementation depends on it. Apps must not link
the sealed consensus core as a shortcut. Signing additionally needs original
branch/context parity and authenticated immutable review/key ownership.
