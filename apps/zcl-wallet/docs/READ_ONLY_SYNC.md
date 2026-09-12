# Read-only sync compatibility checkpoint

This is a design checkpoint, not implemented networking or validated balance
support. No endpoint was contacted, no address was queried, and no node ran on
the development server. The next implementation gate remains
[`NEXT_MILESTONE.md`](NEXT_MILESTONE.md).

## Pin the network and protocol separately

Consensus reference: original Zclassic `v2.1.2-beta6`, commit
`14a83d510ffd109d3fa09bf74ebf8c28854a263f`, `src/chainparams.cpp`.

| Property | Mainnet | Testnet |
| --- | --- | --- |
| P2P magic | `24e92764` | `fa1af9bf` |
| P2P port | 8033 | 18033 |
| Overwinter / Sapling activation | 476969 | 20 |
| Bubbles activation | 585318 | 6350 |
| Difficulty adjustment activation | 585322 | No activation |
| Buttercup activation | 707000 | 78856 |

The P2P ports are not Electrum service ports. Mainnet genesis is
`0007104ccda289427919efc39dc9e4d499804b7bebc22df55f8b834301260602`;
testnet genesis is
`03e1c4bb705c871bf9bfda3e74b7f8f86bff267993c215a89d5795e3708e5e1f`.

The historical [Electrum Zclassic client](https://github.com/ZclassicCommunity/electrum-zclassic/tree/854b91c9bc3cf93e814596a417b474c584c7f589)
at commit `854b91c9bc3cf93e814596a417b474c584c7f589` declares client v3.2.7 and
protocol 1.2 in `lib/version.py`. Its `lib/network.py` subscribes with
`blockchain.headers.subscribe` parameters `[true]`, requests
`blockchain.block.headers` with `[height, count]`, and uses scripthash history
and subscriptions. Modern Bitcoin Electrum protocol behavior cannot simply be
assumed compatible. Server availability and current protocol support have not
been measured.

## The old client is only a transport reference

Its `lib/blockchain.py` uses header lengths 1487 and 543 around Bubbles, but its
`verify_header` returns early for testnet after the previous-hash check. On
mainnet that routine uses fixed difficulty tables for some transition windows
and does not itself call an Equihash solution verifier. Its testnet constants
also reuse mainnet's Overwinter height, unlike original beta6. These behaviors
must not become mobile validation rules. Original node rules and independent
fixtures must qualify any future C verifier. The observed original-beta6
rejection at block 478544 remains undiagnosed until the added diagnostic is
collected; no exception for that block is authorized.

## Proposed C transport boundary

The [ElectrumX protocol basics](https://electrumx.readthedocs.io/en/latest/protocol-basics.html)
describe newline-delimited JSON-RPC over TCP/TLS and script hashes formed by
SHA-256 of the binary scriptPubKey, reversed and hex encoded. The
[protocol methods](https://electrumx.readthedocs.io/en/latest/protocol-methods.html)
describe balance values in minimum currency units and subscriptions that may
omit intermediate headers. These are general protocol references; a selected
Zclassic service must be qualified against its exact negotiated version.

Keep framing, parsing, TLS/socket ownership, deadlines, bounded retries and
request state in C. Bound frames, nesting, counts and outstanding requests;
reject ambiguous duplicate fields and invalid integers. Use checked integer
money arithmetic and explicit handling of signed pending deltas. Test skipped
tips, reorgs, disconnects, malformed data and mismatched network identity with
local public fixtures before selecting a real service.

Android may supply public trust material through a narrow platform adapter.
Its cleartext manifest setting alone is not a TLS control for raw C sockets.
Server-advertised genesis, history and balance are claims, not validation
proofs. Show stale, unavailable and unverified states honestly. This read-only
module must have no spending-key, entropy-generation or signing callback.
