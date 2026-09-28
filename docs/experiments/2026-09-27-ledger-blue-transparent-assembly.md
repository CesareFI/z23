<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Transparent transaction assembly after Blue review

Local time: 2026-09-27T11:49:19-04:00

UTC: 2026-09-27T15:49:19Z

The C23 host assembler accepts a complete unsigned, all-transparent
Sapling-v4 wire and one verified P2PKH signature per input. Each signature
must carry the expected input index and exact preflight ZIP-243 digest.
It replaces only the zero-length input script fields with canonical DER,
`SIGHASH_ALL`, and the compressed public key. Output bytes, lock time, and
expiry remain in the same order. It rejects nonempty input scripts, shielded
sections, missing or reordered signatures, digest mismatches, and
insufficient output capacity before returning a signed wire.

The full host simulation reviewed two outputs, bound a real previous
transaction to its outpoint, derived the input fee and ZIP-243 digest,
simulated a separate touchscreen signing approval, signed that digest with a
real secp256k1 key, verified the Blue-formatted reply, and assembled a
complete v4 transparent wire. The test parsed the assembled wire, checked
the script's DER and public-key pushes, and compared every byte after the
input section with the reviewed unsigned wire. A separate two-input
synthetic layout fixture checked both script positions and rejection of
reordered signature records. That layout fixture is structural; its sample
signatures are not valid payment authorizations.

At 2026-09-27T11:49:19-04:00 (2026-09-27T15:49:19Z), focused
`blue-payment-review` and `blue-payment-sign` tests passed 2/2 under Clang
22.1.6 Debug with AddressSanitizer and UndefinedBehaviorSanitizer and 2/2
under GCC 16.1.1 Release on an AMD Ryzen 7 PRO 8840U. The repository
complexity gate passed 60,846 functions at cap 15. The preflight parser
correctly refuses to process the signed wire as an unsigned draft.

The physical Blue Wallet 0.2.18 still has no routed signing command or
signing approval screen. The host assembly path has not been used to
broadcast a ZCL transaction. Chain inclusion, spendability, branch
activation, actual BOLOS signatures, and Sapling spends remain unverified.
