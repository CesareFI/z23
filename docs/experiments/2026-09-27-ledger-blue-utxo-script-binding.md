<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Ledger Blue UTXO script binding

Local time: 2026-09-27T06:25:44-04:00

UTC: 2026-09-27T10:25:44Z

## Question

Can the host detect when a supplied previous transaction has an output
script of the expected length and value, but its bytes differ from the local
node's current UTXO entry?

## Result

Z23's `gettxdetail` now returns `script_sha256` for each unspent output,
computed from the script stored in chainstate. The C23 Ledger host hashes
the selected script from the supplied previous transaction and requires an
exact lowercase SHA-256 match, in addition to the existing txid, output
index, value, script length, confirmation, unspent status, and coinbase
maturity checks. A missing or duplicate script hash is rejected.

A unit fixture with a 25-byte P2PKH script was accepted with its actual
hash and rejected after changing one script byte without changing length.
The parser also rejected a missing, incorrect, or duplicate hash field.
The read-only wallet CLI integration fixture initially failed because it
spends a different 25-byte script. After the RPC fixture was set to that
script's measured hash, the matching case passed and a deliberately wrong
hash failed before USB access. Both Clang 22.1.6 Debug with
AddressSanitizer and UndefinedBehaviorSanitizer and GCC 16.1.1 Release
passed the two focused tests. GCC 16.1.1 and Clang 22.1.6 compiled
`chain_inspect_controller.c` in strict ISO C23 syntax-only mode with
`-Wall -Wextra -Werror -pedantic`. CPU: AMD Ryzen 7 PRO 8840U with Radeon
780M Graphics. Test date: 2026-09-27.

The complete Ledger host suite passed 30/30 tests on each compiler before
the final parser helper split. After the split, both compilers rebuilt the
affected binaries and passed the two focused tests again. The repository
cyclomatic-complexity ratchet passed across 60,762 functions in 4,430 files.

## Limit

The RPC source change has not been exercised on a running node built from
this commit. A matching script hash binds the supplied wire to one local
UTXO entry; it does not independently prove the node agrees with external
peers or that the selected transaction will remain unspent before broadcast.
The Blue wallet remains read-only and cannot sign a payment.
