# Read-only sync compatibility checkpoint

The C read-only request builders, framing and reply parsers are implemented.
The one-attempt C synchronization state is implemented and tested offline.
TLS remains [blocked for further security review](TLS_REVIEW.md).
Android balance integration and lightweight
validation remain unfinished. No endpoint was contacted, no address was
queried, and no node ran on the development server. The next implementation gate remains
[`NEXT_MILESTONE.md`](NEXT_MILESTONE.md).

## One-attempt synchronization state

`zcl_sync.h` owns a copied address/network, one outstanding request and six
reserved IDs. Requests proceed through version, features, original genesis,
tip, address balance and final tip. The address script hash is withheld until
the identity replies and initial tip parse successfully. This is compatibility
screening; a malicious server can echo the expected genesis. The eventual
adapter must independently require authenticated transport and explicit source
selection before sending anything.

A report is available only after all six responses, with equal before/after
tip height and hash. A changed tip, notification, unexpected ID, malformed
response, error, cancellation or disconnect invalidates the attempt and clears
candidate amounts. Buffer-capacity errors before transmission are retryable;
no network retry or reconnection occurs in this API. It retains no pointers,
allocates nothing and accesses no keys, sockets or persistent storage.

Even a successful report is an unverified server claim for one transparent
address. It supplies no account total, spendable UTXOs, history completeness,
chain proof or signing authority. A same-tip balance can change with mempool
activity; the check does not create an atomic server snapshot. Deadline,
freshness and late-callback handling are supplied by the C watch below. Android
presentation and qualified transport remain separate work.

## Foreground balance lifetime

`zcl_sync_watch.h` composes the attempt with caller-supplied monotonic time,
one nonwrapping attempt token, one cached report and one selected source's
opaque configuration ID. Its caller owns the lifetime on one worker. This is
an offline state API; it contains no thread, socket, timer or JNI handle.

An explicit begin admits one attempt with a deadline of at most 30 seconds,
covering connection setup and every reply. Request/reply calls and snapshot
polls expire it at the deadline. A late token cannot advance the current clock,
cancel another attempt or publish a report. The eventual adapter must still
close connections and enforce I/O interruption; calling this API alone cannot
interrupt an OS call. It must use elapsed monotonic time that includes sleep,
not wall time or a clock that pauses when the device suspends.

| Display state | Meaning |
| --- | --- |
| Unavailable | No completed report in this owner lifetime; amount fields are zeroed and must not be presented as a zero balance. |
| Unverified | Completed server statement, younger than 60 seconds, no intervening failure or refresh. This never means verified funds or spendable value. |
| Stale | Prior statement during refresh, after offline/error/cancellation, or at age >=60 seconds. Its age and last error remain explicit. |

A backward clock clears even cached reports and fails the attempt. A subsequent
explicit retry can establish a new observation in the new clock epoch. Close
clears all owned state; recreation starts unavailable. Source/address/network
changes require a new owner and discard the old cache. Configuration identity
is caller metadata, not authentication. The adapter must reject callbacks from
a destroyed owner before checking numeric tokens, which can start again in a
new lifetime. No UI snapshot is a serializable native state or restart authority.

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

## Implemented C protocol profile

`zcl_electrum.h` provides five request types: `server.version` (requesting
protocol 1.2), `server.features`, `blockchain.block.headers` with `[0,1]`,
`blockchain.headers.subscribe` with `[true]`, and
`blockchain.scripthash.get_balance`. No signing, key, broadcast, socket or
logging operation is in these functions. Output requests have an exact length
and one final LF. Each ID is an explicit nonzero uint32.

The genesis request deliberately uses the plural block-headers method:
[the protocol history](https://electrumx.readthedocs.io/en/latest/protocol-changes.html)
places the singular `blockchain.block.header` method in protocol 1.3. Server
availability and agreement with this legacy Zclassic profile still require
measurement before a service can be enabled.

Frames are limited to 16384 bytes, JSON containers to eight levels, tokens to
128, and decoded object keys to 256 printable ASCII bytes (including complete
DNS names in feature-host maps). The parser rejects
duplicate keys (including escaped aliases and nested extension fields),
malformed UTF-8/escapes/surrogates, trailing data, wrong reply IDs, mixed
notification/reply envelopes and invalid numeric representations. Unknown
bounded extension fields may be ignored. Batches are unsupported. Notifications
need separate session handling; they cannot be mistaken for successful replies.
The line accumulator consumes at most one line per call and preserves its
ready buffer until reset. Overflow is sticky; resetting clears owned bytes.

Money uses checked zatoshi integers. Confirmed value is 0..MAX_MONEY; the
pending value is a signed delta within +/-MAX_MONEY; their sum must remain
0..MAX_MONEY. Output is a server-reported balance, not spendable UTXOs or proof
of inclusion. Public output arguments remain unchanged on failed parsing.

Mainnet/testnet genesis headers in `native/tests/electrum_genesis_fixture.h`
were serialized offline using original beta6 constructors, transaction Merkle
root calculation, `CBlockHeader` and `CDataStream`. The generator checked both
original genesis hashes before emitting the fixtures. The C parser independently
recomputes SHA256d over those exact bytes. Header serialization expects canonical
CompactSize and the original 1344/400-byte solution sizes around the separately
pinned Bubbles heights. Synthetic fork-boundary fixtures test size selection;
they are deliberately not valid Equihash solutions. No proof-of-work,
difficulty, ancestry, Merkle inclusion or consensus validity is established by
this serialization/hash check.

The allocation-free `zjsonp` lexer and `zutf8` package are reused from
`contexts/commons/packages`, with hashes in `native/json-provider.sha256`.
The observed package-source revision is
`de043e0465ef6cccdb33320347565d71455dfd68`. No node/core code is linked.
Review found that the provider's string decoder re-encodes raw non-ASCII UTF-8
bytes incorrectly. This wrapper only uses decoded output for printable ASCII
protocol fields/keys; valid Unicode extension values are syntax-checked and
ignored, never displayed or returned. Tests cover raw/escaped non-ASCII keys
being rejected and ignored Unicode values being accepted. Do not reuse this
ASCII adapter as a Unicode display decoder. Provider integer/float conversion
helpers are unused; the C wrapper implements checked integer conversion.

Host fuzzing exercises all reply parsers and fragmented/multiple LF-delimited
frames, with output-preservation and money invariants. Reproduce from this app:

```sh
cmake -S native -B native/build/fuzz-electrum -DCMAKE_C_COMPILER=clang-20 \
  -DZCL_SANITIZE=ON -DZCL_FUZZ=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build native/build/fuzz-electrum -j4 --target fuzz_electrum seed_electrum
mkdir -p native/build/fuzz-electrum/corpus native/build/fuzz-electrum/artifacts
(cd native/build/fuzz-electrum/corpus && ../seed_electrum)
native/build/fuzz-electrum/fuzz_electrum native/build/fuzz-electrum/corpus \
  -max_total_time=300 -max_len=16385 -rss_limit_mb=512 -malloc_limit_mb=32 \
  -timeout=5 -artifact_prefix=native/build/fuzz-electrum/artifacts/
```

Seed data contains public fixed genesis/header/protocol fixtures only. This
does not test actual TLS, socket cancellation, stale data or reorg behavior.

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

## Foreground owner identity

`zcl_sync_owners` supplies a caller-owned pool of four public balance watches.
Each successful open receives a positive lifetime ID that fits Java long.
Recycling a slot or clearing all watches never resets the issued-ID counter.
Exhaustion refuses instead of wrapping. Failed opens leave the pool and output
unchanged; close clears the slot's public address, report and attempt state.

This is the C boundary for the future JNI owner registry, not a Java pointer
handle. A callback must look up its original lifetime ID, then use its attempt
token on that watch. A recreated watch may issue the same numeric attempt token
as its predecessor; the preceding lifetime ID will still be rejected. The
adapter must serialize lookup, borrowed-watch use and closure under one lock
or worker. No borrowed C pointer may escape the synchronous call or cross JNI.
Only the enclosing adapter initializes the pool, once; clearing it through the
API retains the ID history. Never serialize or restore this native struct.

The current slice adds no registry globals, JNI binding, socket or background
job. Tests exercise full capacity, rejected allocation, closure/reuse, matching
attempt tokens across distinct owners, clear-all and ID exhaustion. The event
fuzzer checks old callbacks against its independent live-owner history.
