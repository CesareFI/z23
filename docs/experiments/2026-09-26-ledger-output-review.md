<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Transparent output review for the Ledger host

Recorded: 2026-09-26T19:28:32-04:00 (2026-09-26T23:28:32Z)

Z23's C23 host transaction review now visits every transparent output after
the complete Sapling-v4 wire passes structural validation. Exact P2PKH and
P2SH scripts yield mainnet ZCL addresses and zatoshi values. OP_RETURN and
unrecognized scripts yield their byte count and SHA-256 fingerprint instead
of a guessed recipient. The CLI shows the first 32 outputs and explicitly
reports truncation when more exist. All output counts remain available in
the summary. Signing remains disabled.

The mainnet address prefixes are `1cb8` for P2PKH and `1cbd` for P2SH in
`core/chainparams/src/chainparams.c`. A known foundation P2SH address from
the same file independently exercised the new Hash160-to-Base58Check path.
A 61-byte synthetic v4 transaction with one 42-zatoshi P2SH output produced
`type:p2sh`, `value_zat:42`, and
`t3Vz22vK5z2LcKEdg16Yv4FFneEL1zg9ojd` in the CLI JSON output.
The 4,118-byte ZIP-243 test vector still produced
`63d18534de5f2d1c9e169b73f9c783718adbef5c8a7d55b5e7a37affa1dd3ff3`.
Clang Debug with sanitizers and GCC Release each passed all ten local Ledger
tests. The dedicated Blue's pinned review app source is unchanged. Its
screen does not yet present individual outputs or authorize a transaction.

Next, the Blue must independently parse the same outputs and show the
recipient, amount, fee, and signing path before any real signing APDU is
enabled. Sapling recipients require separate shielded note handling; an
encrypted Sapling output cannot be inferred from the public transaction
wire. P2SH does not reveal a multisig threshold without its redeem script.
