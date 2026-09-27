<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Ledger Blue review: local node branch input

Date: 2026-09-27.

The read-only review CLI now invokes an explicit absolute-path C23 `zcl-rpc`
executable with `getblockchaininfo`. It caps the RPC client's curl request at
five seconds and the complete child response at seven seconds and 64 KiB.
It rejects non-main networks, JSON-RPC errors, missing or duplicate required
fields, malformed block hashes, mismatched block and header heights, an
initial-block-download flag, and heights before Sapling activation. It
derives the ZIP-243 branch for the node's next height before opening the Blue.

The local node tip is a stronger input than a typed decimal height. Matching
local blocks and headers does not prove that the node has current independent
peers or that a supplied previous transaction is in the active UTXO set.
The Blue still says `BRANCH UNCHECKED` because it does not authenticate the
RPC response itself. The driver remains read-only and cannot sign.

Clang 22.1.6 Debug with AddressSanitizer and UndefinedBehaviorSanitizer passed
26/26 local tests. GCC 16.1.1 Release passed 26/26. The mock RPC test covers
subprocess success, failure, and seven-second timeout plus duplicate, malformed, out-of-sync, and non-mainnet
responses. A locally compiled C23 `zcl-rpc` query returned `Connection failed
(port 18232)` on this laptop; no live-node or Blue review was claimed.
