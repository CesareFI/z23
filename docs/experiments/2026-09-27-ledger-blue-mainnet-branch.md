<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Ledger Blue mainnet branch selection

Date: 2026-09-27.

The read-only `zcl-blue-wallet-review` driver previously accepted any eight
hexadecimal digits as its ZIP-243 branch ID. A mistyped value could produce
a digest for no ZCL mainnet epoch. The driver now accepts a decimal intended
next height and selects the branch from the mainnet activation schedule.

The schedule is transcribed from `core/chainparams/src/chainparams.c` and
`core/params/src/upgrades.c`: Sapling at 476969 selects `76b809bb`, Bubbles
at 585318 selects `821a451c`, and DiffAdj at 585322 selects `930b540d`.
Buttercup at 707000 retains `930b540d`. Heights before Sapling are rejected
because this Wallet review protocol handles version 4 ZIP-243 transactions.
Boundary tests cover the height immediately before and at each branch change.
Clang 22.1.6 Debug with AddressSanitizer and UndefinedBehaviorSanitizer passed
25/25 local tests. GCC 16.1.1 Release passed 25/25 local tests. Both builds
used `-Wall -Wextra -Werror -pedantic`. The CLI also rejected a hexadecimal
branch ID and height 476968 before opening any transaction file.

The caller supplies the intended height. No node tip, consensus parameters,
chain inclusion, or UTXO state is authenticated by this change. The Blue
still displays `BRANCH UNCHECKED`, and this driver still cannot sign.
