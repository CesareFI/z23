<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Transparent-input ZIP-243 digest

Recorded: 2026-09-26T20:06:25-04:00 (2026-09-27T00:06:25Z)

Z23's C23 ZIP-243 library now computes SIGHASH_ALL for a selected Sapling-v4
transparent input. It commits to the transaction's prevouts, sequences,
outputs, shielded data, branch ID, selected outpoint, scriptCode, spent-output
amount, and selected sequence. It rejects a missing input, null script with a
nonzero length, and an amount outside ZCL's public money range. It does not
establish that the caller's scriptCode or amount came from the real UTXO.

The 245-byte transaction in [ZIP 243 test vector 3](https://zips.z.cash/zip-0243)
with input zero, the specified 25-byte P2PKH scriptCode, 50,000,000 zatoshi,
and branch `0x76b809bb` produced
`f3148f80dfab5e573d5edfe7a850f5fd39234f80b5429d3a57edcc11e34c585b`.
Changing the amount, scriptCode, or branch changed the digest. The existing
4,118-byte shielded test vector still produced
`63d18534de5f2d1c9e169b73f9c783718adbef5c8a7d55b5e7a37affa1dd3ff3`.
Clang Debug with sanitizers and GCC Release each passed 12 of 12 local
Ledger tests, and the repository complexity gate passed at its cap of 15.

The shared Blue Review source was rebuilt as version 0.3.1, with a 23,552-byte
`.text` image, zero `.data`, and 6,068-byte `.bss` including its reserved
stack. Its SHA-256 is
`da4f6671eaa41b3a1c94ec5f37936fc78f4845ec96ac8d84c0f94aa37b862d08`.
The new transparent function is not exposed by the Review app. Version 0.3.1
has not been installed or physically tested. No signing command was added.
