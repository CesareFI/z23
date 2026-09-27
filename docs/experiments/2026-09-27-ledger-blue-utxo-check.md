<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Ledger Blue review: local UTXO check

Date: 2026-09-27.

Z23 exposes `gettxdetail` for a transaction present in its local UTXO view.
The read-only Blue review CLI now queries it for every hash-bound input after
deriving the previous-transaction output and ZIP-243 digest. The response must
name the requested transaction, report that exact output as unspent, match
its decimal amount and script length, and give a creation height before the
node's selected next height. Coinbase outputs must be at least 100 blocks old
at that height, matching `COINBASE_MATURITY` in Z23 consensus.

All amount conversion uses eight decimal digits and integer zatoshi arithmetic.
Missing, duplicate, spent, immature, malformed, or mismatched data stops the
review before the Blue is opened. The output script bytes remain bound by the
supplied previous transaction's SHA-256d txid; `gettxdetail` reports script
length but does not return the script itself.

This establishes local-node UTXO evidence only. The RPC calls are separate
reads, so a concurrent reorganization can change the view between inputs.
Local headers matching local blocks do not prove an independently current
network tip. Ownership, change, and final signing approval remain unproved;
the device app remains read-only and uninstalled.

Clang 22.1.6 Debug with AddressSanitizer and UndefinedBehaviorSanitizer passed
26/26 local tests. GCC 16.1.1 Release passed 26/26. Parser tests rejected
spent outputs, changed amounts, changed script length, future heights,
immature coinbase outputs, and duplicate output indexes. The CLI integration
test rejected a simulated spent UTXO before opening a Blue device. The
cyclomatic complexity gate passed at cap 15. No live node answered RPC on this
laptop, so local-node UTXO results remain unverified outside simulation.
