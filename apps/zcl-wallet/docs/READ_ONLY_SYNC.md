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

The explicit `zcl_sync_start_with_history` and
`zcl_sync_watch_init_with_history` profiles insert one bounded history query
between balance and final tip, reserving seven IDs instead of six. This choice
is fixed for the watch's lifetime. The default initializers remain balance-only.
The combined report publishes only after every response succeeds; malformed or
oversized history, changed final tip, and positive history heights beyond the
initial tip invalidate the attempt and clear its entire candidate. The existing
source/address ownership, elapsed deadline, stale prior report, late-token
refusal, clock-rollback clearing and empty restart rules also cover history.
`has_history=false` means no history query was included; `true` with count zero
means the server asserted an empty array. Neither establishes completeness.

Even a successful report is an unverified server claim for one transparent
address. It supplies no account total, spendable UTXOs, history completeness,
chain proof or signing authority. A same-tip balance or history can change with mempool
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

## Offline processing benchmark

From `apps/zcl-wallet`, build and run the public-fixture timing tool explicitly:

```sh
cmake -S native -B native/build/sync-benchmark \
  -DCMAKE_C_COMPILER=clang-20 -DCMAKE_BUILD_TYPE=Release \
  -DZCL_SANITIZE=OFF -DZCL_ORACLE=OFF -DZCL_FUZZ=OFF \
  -DZCL_TLS_REVIEW=OFF -DZCL_JNI=OFF
cmake --build native/build/sync-benchmark --target bench_read_only_sync -j4
timeout 60s native/build/sync-benchmark/bench_read_only_sync
```

Use a separate directory with `-DCMAKE_C_COMPILER=gcc` for comparison. Preserve
compiler, build profile and host details with measurements. Sanitizer execution
checks correctness separately; it is not a release-performance comparison.

The tool covers original mainnet/testnet genesis fixtures, balance-only and
two-entry history profiles, and 1/256/4096-byte response fragments. Each of its
12 profiles warms up for ten complete attempts and measures five batches of
100 attempts. Every attempt constructs six or seven requests, frames and parses
all replies, publishes a snapshot and verifies expected public report fields.
Timing includes those correctness checks. Wall and thread CPU microseconds are
reported separately; there is no timing acceptance threshold. The benchmark
owns one checked fixed-size workspace allocation outside the measured batches,
prints its size, and frees it on success or failure. Fixture generation and
printing occur outside each measured batch.

There is no network, TLS, wallet, storage, Android scheduling or chain validation
in this measurement. Simulated monotonic timestamps qualify the local state
path only; they do not measure real I/O deadlines. Results cannot establish live
sync throughput, device responsiveness, battery use, or server truth. Compare
like-for-like runs before changing any implementation, and keep the existing
validation, refusal, deadline and freshness contracts authoritative.

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

`zcl_electrum.h` provides six request types: `server.version` (requesting
protocol 1.2), `server.features`, `blockchain.block.headers` with `[0,1]`,
`blockchain.headers.subscribe` with `[true]`, and
`blockchain.scripthash.get_balance` and `blockchain.scripthash.get_history`.
No signing, key, broadcast, socket or
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

### Bounded history codec

The history codec is optionally composed by the C sync/watch and explicit JNI
owner; its history view is fixture-only and it has no persistence or enabled endpoint.
It accepts an array of at
most 16 unique 32-byte transaction IDs and claimed heights, preserving server
order. More entries return `RESOURCE_EXHAUSTED`, never a truncated success.
The existing 128-token limit also applies: 16 entries with a numeric fee fit,
but sufficiently complex extension fields can exhaust the budget sooner.
This initial operational cap makes heavily used addresses unavailable through
this codec until a separate bounded retrieval design is qualified. It is not a
consensus rule, pagination support or a claim of complete transaction history.

Fields and duplicate refusal follow the pinned client's
[`Synchronizer.on_address_history`](https://github.com/ZclassicCommunity/electrum-zclassic/blob/854b91c9bc3cf93e814596a417b474c584c7f589/lib/synchronizer.py#L104).
Its [`wallet.py` constants](https://github.com/ZclassicCommunity/electrum-zclassic/blob/854b91c9bc3cf93e814596a417b474c584c7f589/lib/wallet.py#L76)
distinguish -1 (unconfirmed parent), 0 (unconfirmed), and -2 (local-only).
Network replies accept -1..INT32_MAX, matching this wallet's supported tip
range; they reject the local-only sentinel. The
[`network.py` request](https://github.com/ZclassicCommunity/electrum-zclassic/blob/854b91c9bc3cf93e814596a417b474c584c7f589/lib/network.py#L653)
uses `blockchain.scripthash.get_history`. These historical files were inspected
as text only; no old client or wallet was executed.

IDs decode from exact 64-hex-character strings, allowing upper/lower case and
valid JSON ASCII escapes. Duplicate detection compares decoded bytes, preventing
case aliases. Unknown bounded fields, including fee, are syntax-checked but
never surfaced as fee estimates or amounts. Every failed reply preserves the
caller's output, including a malformed entry after valid entries. Empty success
means only an empty server assertion. No transaction bytes, inclusion proof,
confirmation count, amount, completeness or spendability is established.

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

`zcl_sync_owners` supplies a caller-owned pool of four public sync watches.
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

The C pool itself has no global state, JNI or background job. Tests exercise
full capacity, rejected allocation, closure/reuse, matching attempt tokens across
distinct owners, clear-all and ID exhaustion. The event fuzzer checks old
callbacks against its independent live-owner history.

## JNI ownership and public snapshots

`jni_sync.c` now explicitly owns one four-slot process registry and its mutex.
Lookup, borrowed-watch use and closure all hold that same mutex. Only positive
never-reused numeric IDs cross JNI; there is no pointer cast, native struct
serialization or retained Java reference. The managed `ReadOnlySync` owner also
serializes its own methods. Each Attempt retains its original managed owner and
token. Close is idempotent through that owner and makes future callbacks refuse.
Owners require explicit close; failure to close consumes a bounded slot until
process exit and eventually refuses creation. No background cleanup thread,
finalizer, automatic retry or socket is introduced.

Java timestamps, request IDs and tokens are checked before unsigned/narrowing
conversion. A deadline must fit positive Java-long time. C remains authoritative
for expiry, stale age, protocol transitions, balance bounds and publication.
The managed owner now takes one elapsed-clock function and samples it inside
its serialized operations. Android must supply `SystemClock.elapsedRealtime`;
an uptime or wall-clock substitute is not valid. Taking a timestamp before
waiting for the owner lock could otherwise deliver an older observation after
a newer one from another thread. Raw timestamp JNI calls remain internal for
the adapter and boundary fixtures. Closing drops the supplied clock reference,
and a closed managed owner never invokes it. The clock function must be quick,
nonblocking and monotonic; expiry/rollback decisions still belong to C.
JNI request delivery publishes waiting state only after creating the returned
array; allocation/region failure aborts the current attempt. Replies use one
checked 16384-byte allocation which is cleared and freed on every path.
Unexpected JNI/provider/mutex errors fail closed, without input text in errors.

The snapshot carries unverified/stale/unavailable state, refresh/fault/age and
the reported confirmed amount, signed pending delta, total and height. Its
`nextChangeDelayMillis` is C's relative delay to an active deadline or fresh
report expiry, bounded to 60000ms. Zero means no time-only transition is pending.
It is a wakeup hint, never a cached decision: a delayed timer must query C again,
and foreground replacement must cancel the timer with its original owner.
No timer or polling worker is created by the core. An
unavailable report becomes null in the managed view. It has no spending or
chain-proof authority. Pending changes use the bounded C signed-delta formatter:
zero is "0", positive changes include "+", and negative changes include "-".
MoneyRange is checked before negation and decimal conversion. The nonnegative
payment-amount parser continues to reject both signs. No floating-point or
locale-dependent amount formatting is used. Source/address remain fixed
metadata on the owner.

`ReadOnlySync.withHistory` opts into the seven-response profile through the
same registry. The original constructor remains balance-only. History snapshots
sample C once under the registry mutex and project balance, freshness and
history together; Java allocation happens after unlocking. The existing
ten-long snapshot is unchanged. The opt-in packet appends availability/count,
then eight unsigned 32-bit hash words and one signed height per entry, bounded
to 156 Java longs. Every hash word fits a positive long, avoiding unsigned
64-bit to signed conversion. The managed adapter checks packet/word/count bounds
and renders the exact hash words as lowercase hexadecimal; protocol and
publication decisions remain in C. Report history is null when no query was
included, an empty list for an empty assertion, or at most 16 public entries.
None of these states supplies an inclusion proof or spending authority.

JNI snapshot errors publish only their status: the remaining words are zero,
with ten words for balance or twelve for history. Partial projection cannot
publish refreshing, balance, age or history metadata alongside an error. Native
clock/deadline updates remain authoritative even if Java publication fails.

Android BalancePresentation owns one sync lifetime, coalesces worker signals
into at most one pending UI redraw, and samples the snapshot at actual delivery.
Create/close/render belong to the same UI thread. Close cancels pending reads,
drops receiver references and closes the C owner, so replacement cannot render
an old session. A failed snapshot closes the owner and delivers only a stable
unavailable status; receiver exceptions close and propagate. Queue rejection
permits an explicit retry. Presentation owns one cancellable main-queue wakeup
using C's delay; a timer callback coalesces a new redraw and never carries a
snapshot. Early/late callbacks recheck C. Failure to arm a required wakeup closes
the owner and reports unavailable before displaying a new unverified result.
Close cancels the wakeup even if its old callback was already captured. Android
Handler uptime schedules only a hint; the owner's elapsedRealtime clock remains
authoritative, including sleep. The platform checks the scheduling addition and
starts no periodic poll. Foreground closure/resume must still replace the owner.
No snapshot is saved, and no endpoint or worker is enabled by this adapter.
The receive screen includes a BalanceView that starts unavailable with no
numeric amount. Complete reports are labeled unverified for this address only;
stale reports retain that label and show that they are outdated. Formatting
comes from C, including signed pending changes. Updating/fault messages never
turn an unavailable result into zero. No real source is connected in the app;
nonempty rendering is currently qualified only with public local fixtures.
The view excludes text from framework saving, ignores restored TextView state
under its ID, and clears amounts when detached. Formatting failure clears the
previous display before propagating to its owner. This is view-state evidence,
not a qualified hardware-authenticated wallet or process-death recovery journey.

The receive screen also includes a HistoryView after the lock/scan controls,
keeping those actions accessible before a long report. It starts unavailable.
Public fixtures can render at most16 exact IDs, pending status or a reported
block height, with explicit unverified/outdated labels and an incomplete-history
caveat. An empty server assertion is distinct from unavailable history or a
balance-only report. No amount, confirmation count, explorer link or spending
action is derived from these entries. Display bounds reject malformed row
strings/heights before publishing the assembled text. Both public report views
clear existing text before formatting an unavailable message. History IDs are
excluded from framework text saving, autofill and content capture; old saved
TextView state and detachment clear the view. Its expiry test uses the existing
foreground presenter and C wakeup hint. ReadOnlyReportViews renders both views
from one snapshot and clears both if either render fails. The nonexported debug
host has an explicit history fixture profile; changing profiles closes the
prior owner, and the choice is never stored in Bundle or read from an intent.
Pause closes the owner and clears both views. Resume starts an empty owner;
recreation defaults to balance-only until a new explicit history fixture call.
The process-relaunch controller supports either public profile and requires a
matching readiness profile and PID before terminating the development app.
No wallet record, key, endpoint or automatic fixture replay participates.
The bounded metadata
query may run on the UI thread; blocking I/O must never hold the sync monitor
or native registry lock.

The same twelve native public fixture frames feed JVM tests and Android test
assets. Host fake-VM tests inject allocation and JNI-region exceptions under
ASan/UBSan/LSan and assert native frame clearing. That fixture is excluded from
the TLS-review build profile. Fake-VM/fuzzer results do not qualify Android VM
behavior; real JVM checks and emulator tests complement them. TLS remains
**BLOCKED — REQUIRES FURTHER SECURITY REVIEW**, excluded from Android/JNI builds.

### Activity lifecycle fixture

WalletDisplayFixtureActivity exists only under the Android debug source set.
It is nonexported, has no launch filter, and uses the actual receive layout with
a public zero-hash mainnet address. It never opens storage, Keystore, a socket or
intent-supplied state. Instrumentation feeds the existing public frames through
the normal C/JNI attempt API. Pause closes the presentation/wakeup/native owner
and clears the view; resume creates an empty owner and does not replay frames.

The API-35 fixture verifies five recreation cycles (exceeding four registry
slots), background/resume with a live old attempt, and a new Activity after
completion. Old attempts refuse and old views lose their displayed amount.
All three device cases completed with zero failures. APK inspection confirms
the host is absent from the release manifest and sync frames remain in the test
APK only. This is public display lifecycle evidence, not positive hardware
custody or process-death recovery qualification.

### Explicit emulator process relaunch

After installing the current debug and test APKs, the separate controller can
terminate only its verified public-fixture development process and test a fresh
process. It requires an explicit emulator serial and a new report directory:

```sh
bash tools/check-process-relaunch.sh "$ANDROID_HOME/platform-tools/adb" \
  emulator-5554 /tmp/zcl-public-process-fixture-new
```

Preparation verifies a complete displayed public report, then announces its PID
and waits with a bound. The controller requires that exact running app PID before
force-stopping `org.zclassic.wallet.dev`, verifies termination, and starts a
separate test process. The new process must have a different PID, show no amount
or replay, and begin an empty protocol exchange. Preparation is intentionally
killed and is never counted as a standalone passing test; its crash/host status
is preserved. The subsequent test must produce an explicit JUnit success.
The fixture also requires emulator hardware and opt-in arguments. It is not part
of ordinary Gradle check and never opens wallet files, keys or network sources.

The API-35 run verified PID 22911, termination, and new PID 22968; the final
test passed in 28.579 seconds. This qualifies clean process relaunch of public
display state. It does not qualify restored OS task state, hardware custody,
interrupted secret persistence or a real synchronized wallet.
