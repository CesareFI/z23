<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Ledger Blue chain-tip IBD gate

Local time: 2026-09-27T06:17:48-04:00

UTC: 2026-09-27T10:17:48Z

## Question

Can the host reject a local Z23 chain-tip reply whose initial download state
is absent or active, while the node reports that state on both resolved and
unresolved tip paths?

## Result

The host now requires an explicit JSON boolean
`initialblockdownload:false` in `getblockchaininfo`. Missing, true, null,
and duplicate fields fail. The existing mainnet identity, block/header
height equality, and stable-tip checks still apply. Z23 already reports
`true` when its provable tip cannot be resolved; its normal response now
reports the node's initial-download predicate explicitly.

Clang 22.1.6 Debug with AddressSanitizer and UndefinedBehaviorSanitizer and
GCC 16.1.1 Release each passed 30/30 local tests, including
`blue-chain-tip` and `blue-wallet-review-cli`. GCC 16.1.1 and Clang 22.1.6
compiled `blockchain_controller_chain.c` in ISO C23 syntax-only mode with
the repository's include paths, `-Wall -Wextra -Werror -pedantic`. CPU: AMD
Ryzen 7 PRO 8840U with Radeon 780M Graphics. Test date: 2026-09-27.

## Limit

The node source change was not exercised against a running, synchronized
Z23 node in this test. A false IBD flag and equal local block/header heights
do not independently establish agreement with peers. The Blue itself does
not verify chain inclusion, UTXO status, or the active consensus branch;
the local node supplies those facts for the read-only preflight. Physical
payment signing remains disabled.
