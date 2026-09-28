<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Host verification of Blue payment signatures

Local time: 2026-09-27T11:37:15-04:00

UTC: 2026-09-27T15:37:15Z

The C23 host verifier accepts one candidate Blue INS `29` reply only when
its status is `9000`, its length is exact, and its input index and fixed
external or internal path match the reviewed input. It requires a compressed
public key whose independently computed HASH160 equals the expected
device-owned input hash. The DER signature must be canonical and low-S.
The caller supplies a cryptographic verifier for the exact device-derived
ZIP-243 digest. On any failed check, the output structure is zero and no
public key or signature is returned to transaction assembly.

The focused test generated a real secp256k1 signature, checked a valid reply,
and rejected truncated status, error status, wrong index or path, wrong DER
length or tag, wrong digest, and wrong public-key hash. It also converted the
valid low-S signature to a cryptographically valid high-S form. OpenSSL
verified that high-S signature, while the C23 reply parser rejected it as
noncanonical. OpenSSL is used only by the test signer and verifier; the host
reply parser links only the C23 DER module and a caller-supplied hash and
verification callback.

At 2026-09-27T11:37:15-04:00 (2026-09-27T15:37:15Z), the focused
`blue-payment-sign` test passed 1/1 under Clang 22.1.6 Debug with
AddressSanitizer and UndefinedBehaviorSanitizer, and 1/1 under GCC 16.1.1
Release on an AMD Ryzen 7 PRO 8840U. The repository complexity gate passed
60,826 functions at cap 15.

The Blue Wallet 0.2.18 image has no routed signing command. This parser does
not establish physical Blue signing, chain inclusion, UTXO status, branch
activation, Sapling signing, or host transaction assembly.
