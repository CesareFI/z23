<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Previous transaction gate for physical Blue review

Date: 2026-09-27. The C23 `zcl-blue-wallet-review --test` command now requires
one complete previous v4 transaction per transparent input. Before opening
the Blue's USB interface, Z23 parses those bytes, computes each SHA-256d
transaction ID, compares the exact input outpoint, selects a standard P2PKH
previous output, rejects duplicate outpoints, and derives the transparent
input total and fee. The existing parser also rejects shielded and
nonstandard outputs on this path. The command reports the fee as host
preflight data and states that chain inclusion and unspent status are
unverified. The Blue does not independently verify the fee yet.

The `blue-wallet-review-cli` integration test generates a complete synthetic
previous transaction and a spend referencing its computed hash. It verifies
three CLI paths: no previous file returns usage status 2; a mismatched file
returns status 1 with an outpoint error; the matching file reaches the
inaccessible-device check without opening USB and returns status 1 for that
reason. Clang 22.1.6 Debug with address and undefined-behavior sanitizers
passed 23/23 host tests; GCC Release passed 23/23. The repository-wide
cyclomatic ratchet passed at cap 15 across 60,397 functions in 4,408 files.
No new Blue image was built or installed for this host-only change.

The next provenance gate is an independently verified node view proving each
outpoint is unspent at the selected chain height, with active consensus
branch ID and ownership bound to the signing account. A device signing path
must independently display the verified input total, all outputs, change,
fee, network, and account before its final approval. This read-only gate
cannot authorize a transaction.
