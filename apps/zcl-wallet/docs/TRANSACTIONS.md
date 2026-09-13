# Transparent transaction development

The C codec handles a bounded transparent-only subset of the existing Zclassic
v4/Sapling wire format. A thin JNI adapter exposes offline unsigned review data;
there is no endpoint, key, signing, broadcast or send-screen entry point. Successful decoding establishes
only the checks listed below. It does not establish valid scripts, signatures,
funding, ownership, fees, expiry at the current tip or chain acceptance.

`zcl_transparent_tx` owns every byte it exposes. There is no borrowed network
span, allocation, callback, global mutable state or retained pointer. Callers
provide stable nonoverlapping objects for each synchronous call. All output
objects, lengths and buffers remain unchanged on failure. Unused parsed fields
start at zero. Scripts are public opaque bytes, including empty scripts; a
nonempty input script does not prove that an input is signed.

## Wire profile and bounds

| Field | Codec contract |
| --- | --- |
| Header | Exactly uint32 little-endian `0x80000004` |
| Version group | Exactly uint32 little-endian `0x892f2085` |
| Inputs | 1..8, unique outpoints, no null outpoint/coinbase |
| Input script | 0..128 opaque bytes |
| Outputs | 1..16, each value and their checked sum <=21,000,000 ZCL |
| Output script | 0..25 opaque bytes; sufficient storage for P2PKH/P2SH |
| Lock time | Exact uint32, without interpreting height versus timestamp |
| Expiry height | uint32 strictly below500,000,000; zero retains its wire meaning |
| Shielded fields | valueBalance=0, empty spend/output/JoinSplit vectors |
| Trailing data | Refused; exactly one transaction must fill the input span |
| Maximum wire | 1925bytes; `29 + 8*(41+128) + 16*(9+25)` |

These input/script/output caps are wallet resource policy, not Zclassic
consensus limits. Legacy transactions, v3, shielded transactions and otherwise
valid larger scripts/vectors are refused. No field is truncated or skipped to
make a larger transaction fit. A subsequent review/signing layer must classify
output scripts and prove change ownership before exposing a send action.

Vector/script lengths use canonical CompactSize. Accepted values fit one
byte. Extended forms are read with exact width, reject nonminimal encodings,
and refuse canonical values above the local cap before any conversion/index.
Money is read as uint64 bits and range-checked before use, so a negative int64
wire encoding cannot enter a valid output. Total addition uses remaining-money
subtraction. All integer fields are assembled explicitly without unaligned
loads, signed shifts or host-endian dependencies.

Outpoint and transaction IDs use displayed big-endian byte order at the API;
outpoint hashes reverse on wire. `zcl_transaction_id` hashes the exact validated
serialization twice with SHA256, then reverses the digest for display. It is
not a signature hash. Signing or otherwise changing any serialized byte changes
the ID; an unsigned construction has no final broadcast identity.

## Pinned original reference and evidence scope

Reference: Zclassic commit
`14a83d510ffd109d3fa09bf74ebf8c28854a263f`, inspected from the preserved original
checkout without running a node. The relevant files are
[`primitives/transaction.h`](https://github.com/ZclassicCommunity/zclassic/blob/14a83d510ffd109d3fa09bf74ebf8c28854a263f/src/primitives/transaction.h),
`primitives/transaction.cpp`, `hash.h`, `serialize.h`, `main.cpp` and
`consensus/consensus.h`. The wire layout/group/header, SHA256d identity,
canonical lengths, output-money sum, nonempty vectors, duplicate/null outpoints
and expiry threshold were checked against those sources. This does not qualify
the original's contextual checks or assume that a historical branch ID is
appropriate for the current chain.

The original `src/test/data/sighash.json` has SHA256
`6e10f3e5649c876a8968a3ba13885aeb8dcee8040fd89e7e39651b041d07f30c`;
`src/primitives/transaction.h` has SHA256
`5296163b58f516c3a0cc2b6f46d55bf2195d371b1ab81a312fb97de48a9c7ece`.
Its random v4 generator always includes shielded fields. Our three fixtures
preserve the exact transparent prefixes through lock/expiry of lines203,208,296
and replace only the shielded tail with eleven zero bytes. These are explicitly
projected fixtures, not untouched original transactions. Original script bytes
remain opaque, and these synthetic outpoints do not represent spendable funds.

`tools/project-original-transactions.sh <original-checkout> <new-report-dir>`
reproduces the projections using pinned Git objects and bounded shell/awk glue.
OpenSSL computes independent SHA256d expected IDs. The report preserves original
rows, projected bytes, IDs and SHA256 manifests. Neither the C codec nor its
serializer generates these expected values. Original-node deserialize/reserialize
and contextual transaction acceptance remain additional independent gates.

The projection source retains the original MIT notice in
[`transaction-reference.LICENSE`](transaction-reference.LICENSE).

Deterministic tests cover all three byte/ID fixtures, every truncation and
undersized output capacity, max-size objects, high-bit IDs and uint32 fields,
empty scripts, malformed/nonminimal lengths, trailing bytes, unsupported tails,
duplicate/null outpoints, excessive counts/scripts, expiry and money overflow.
Canaries and prefilled outputs check failure atomicity. The bounded fuzzer
mutates both wire input and caller-owned unsigned-integer/byte objects, checks
failure atomicity and requires canonical byte equality after successful parsing.

## Previous outputs and destination templates

`zcl_transaction_prevout` parses supplied previous-transaction bytes into owned
storage, derives that exact canonical transaction ID, compares it to the input's
display-order ID, then checks the uint32 output index before copying its value
and script. A caller cannot substitute an amount, another transaction or a
different indexed output while retaining the expected outpoint. The previous
transaction must fit this codec's subset; larger, legacy and shielded funding
transactions remain unsupported. Failure leaves the output unchanged.

This comparison establishes byte/hash/index consistency. It does not establish
that the previous transaction occurred on the selected chain, remains unspent,
is mature, or belongs to the wallet. Inputs themselves must later be bound to
an authenticated wallet review and qualified chain evidence. A remote balance,
history row or arbitrary caller-provided outpoint cannot authorize spending.

`zcl_address_from_script` recognizes only the exact P2PKH and P2SH templates,
including their canonical direct20-byte push and exact final opcodes. Unknown,
longer, shorter or alternative pushdata forms refuse without changing output.
All20 hash bytes are opaque. The caller supplies the selected network because
transaction scripts do not encode one; this API neither discovers a network nor
proves key/redeem-script ownership. Existing address-to-script encoding remains
the inverse for these two templates.

Fixtures cover both networks/kinds, every opcode-byte substitution, every hash
byte value, all shortened/extended lengths and failure canaries. Independent
projected previous-output values/scripts, every fixture-byte mutation, wrong
hashes, invalid indexes and synthetic standard destinations exercise extraction.
The transaction fuzzer also checks script round trips and successful/mismatched
previous-output extraction from each accepted transaction.

## Bounded transaction assessment

`zcl_transaction_assess` supplies public review data from the current bounded
transaction and exactly one previous-transaction descriptor per input. Each
descriptor supplies a stable span only for the call; different inputs may
share the same previous bytes when they select distinct output indexes. The
assessment output must not overlap any input. No borrowed pointer escapes.

Every source passes the hash/index check above, and every selected input and
current output must have an exact P2PKH/P2SH destination template. Unknown
destinations refuse the whole assessment; they cannot disappear from the total
or be mislabeled as change. Input and output totals use checked integer money
addition. Output total must not exceed input total before fee subtraction.
The caller must provide an explicit absolute fee ceiling within0..MAX_MONEY;
the computed fee must fit it. No remote estimate, fee rate or default allowance
is inferred. A zero ceiling accepts only zero fee under these numeric checks;
that does not establish relay acceptance.

The1056-byte host assessment owns bounded input/output destination/value rows,
totals, fee, fee ceiling, selected network, current serialized size and exact
current transaction ID. It includes no ownership/change/verified/approved flag.
Size describes current serialized bytes, including any current input scripts,
and is not a forecast of a later signed transaction's size. Later mutation does
not change already returned rows, but requires a new assessment before use.

The transaction ID binds transaction bytes. It does not bind the selected
network, fee policy, source inclusion/unspentness or user consent. A future
immutable review/signing lifetime must retain and check those separately. This
API cannot establish wallet ownership merely because a caller supplied a
matching destination. No assessment, JNI or UI route signs or broadcasts.

Synthetic fixtures cover exact500-zatoshi fee boundaries, output/input money
overflow, insufficient funds, zero/max ceilings,8-input/16-output boundaries,
distinct versus duplicate outpoints, wrong source order/count, unknown
destinations and unchanged outputs after failures at later inputs. Tests mutate
and truncate every byte of both funding fixtures. A dedicated bounded fuzzer
uses fixed public funding and dynamically matched previous transactions to
exercise complete assessments, fee limits, row totals and failure atomicity.

## Immutable unsigned review lifetime

`zcl_review_owner` retains one prepared unsigned draft, its exact canonical
bytes, selected network and assessed absolute fee policy. Opening parses an
owned transaction, rejects every nonempty input script, completes the previous
output assessment, then serializes that same owned object before publishing a
positive review ID. Caller mutation after opening cannot change the draft.
Returned snapshots and byte buffers are independent copies. A read exposes
review data only; it is not consent or authority to sign or broadcast.

Each snapshot also owns the exact uint32 lock time and expiry height, plus
display-order previous-transaction ID, uint32 output index and uint32 sequence
for every input. Rows share the assessment's input count/order, and unused
rows remain zero. The context comes from the same owned parsed transaction
used for assessment and serialization. These are raw fields; no boolean
finality, current-tip expiry, confirmation or replaceability claim is derived.
All outputs remain explicit assessed destination/value rows without a change
label. Context and accounting publish in one copied snapshot.

Initialize the caller-owned state once with `{0}` per enclosing adapter
lifetime and serialize every operation under the same lock. No borrowed
pointer, native heap or background worker survives a call. IDs increase through
INT64_MAX without reuse, including across clear/cancel; exhaustion refuses.
Never copy/reset an owner while callbacks can retain its IDs. The JNI adapter
uses one process-lifetime owner and never resets its issuance counter. IDs must
remain within that process and must never be restored from external state.

The fixed lifetime is90,000ms from a trusted elapsed-monotonic opening time.
An opening that would overflow its uint64 deadline refuses. Every snapshot or
byte read checks identity before time: a late callback cannot read or expire a
replacement review. A valid ID expires at the deadline, and clock rollback
cancels it. Neither reads nor failed small-buffer copies extend the deadline.
Returned remaining time is only a scheduling hint; delivery must read again.

Active state returns BUSY even if its deadline has elapsed, until a read
observes expiry or the caller explicitly cancels/clears. Failed preparation
leaves the owner and output ID unchanged. Read failures leave caller outputs
unchanged; a valid read with insufficient capacity still samples the clock.
Cancellation, observed expiry, rollback and foreground teardown clear all
retained draft bytes without resetting the issuance counter. Do not persist or
restore this state from disk, Bundle or intents; restart has no active review.

The review owns public transaction metadata, not keys or authenticated wallet
ownership. Supplied previous bytes remain unqualified for chain inclusion,
unspentness or maturity. P2SH recognition is not redeem-script ownership.
Change classification, chain/branch context, hardware authentication and
original signature hashes are separate gates. No review Activity is wired yet;
the review cannot enable the quarantined transport or BLAKE2 candidate.

Deterministic cases cover caller/copy mutation, every draft truncation and
undersized copy capacity, late callbacks after repeated replacements, exact
deadline and rollback transitions, time/ID exhaustion, signed-script refusal,
failed source/fee preparation and zeroed cancellation state. The sequence
fuzzer compares public operations with an independent fixed-duration model
using subtraction from the opening time, bounded to64 operations per input.
Context fixtures cover high-bit lock/sequence values, zero and maximum accepted
expiry, all eight rows with distinct output indexes through15, and a smaller
replacement leaving no stale extra rows. Fuzz inputs vary full-width lock and
sequence fields and bounded expiry while exercising the same lifetime model.

## JNI and managed ownership

The JNI adapter serializes one process-wide unsigned review under a mutex.
Opening another active draft returns BUSY. It copies the draft and at most
eight previous Java byte arrays, each bounded to1925bytes, into one checked
fixed native allocation. No Java array is pinned and no reference is retained;
each temporary element reference is deleted, and the entire allocation is
zeroed before free on success or failure. Only a positive nonreused ID escapes.

Snapshots encode at most268 longs:20 header fields,17 per input and7 per
output. Header fields carry status, remaining time, network, lock/expiry,
serialized size, counts, totals, fee/ceiling and eight transaction-ID words.
Input rows carry eight previous-ID words, index, sequence, value, kind and five
address-hash words. Output rows carry value, kind and five hash words. Hash
words are unsigned32 values represented exactly as positive longs; no signed
64-bit hash reinterpretation occurs. Failure packets contain one status only.

Java result allocation happens after native unlocking. A failed allocation or
region write attempts cancellation only for the sampled ID, so failure cleanup
cannot reach a replacement. The deterministic fake VM interleaves replacement
at this exact boundary and checks both unlocked allocation and stale cleanup.
Mutex failures return an I/O fault; an exceptional mutex failure can require
process restart rather than safely claiming that a draft was cleared.

`UnsignedReview` retains only its ID and clock. Reads sample the trusted elapsed
clock inside managed serialization and close on clock/native/decode failure.
Close drops the clock and checks the native cancellation result; construction
failure cancels the newly issued ID. Returned rows use immutable values and
unmodifiable lists; unsigned byte copies belong to the caller. A caller must
explicitly close the owner on background, lock or replacement. Abandoning it
without close can leave BUSY state until process restart; GC is not a lifecycle
mechanism. A queued display must re-read its original foreground owner.

Destinations expose network, kind, hash and value. `Destination.address()` now
uses the thin JNI/managed factories for canonical P2PKH/P2SH text, with encoding
in C through `zcl_address_encode`. The P2SH factory owns its copied public record
and does not establish redeem-script ownership. `ReviewView` displays these full
addresses beside every output and each supplied funding destination.
The encoder validates explicit network/kind and reuses the same version prefixes
as parsing. The existing public-key-hash receiving helper delegates without
changing its P2PKH behavior. Four public address/script vectors from the pinned
original `src/test/data/base58_keys_valid.json` qualify the exact text/hash
mapping; arbitrary hashes, every short capacity and a bounded encode/parse
fuzzer supplement these independent vectors. Outputs have no appended NUL and
remain unchanged on failure.
No label asserts change, inclusion, unspentness, maturity or wallet ownership.
There is no authentication, signing, broadcast, persisted review or automatic
network operation in the adapter.

Public fixtures are shared from `wallet-core/src/test/resources/review` into
the Android test APK. The package gate refuses them in application APKs and
requires all three binary fixtures in the test APK; negative listing mutations
exercise both refusals. The C fixture generator refuses existing files and
independent OpenSSL SHA256d identifies the draft. Real JVM `-Xcheck:jni` and
API35 emulator tests qualify transfer, cancellation, expiry and owned copies;
they do not qualify a review Activity or original-node transaction acceptance.

## Foreground presentation ownership

`ReviewPresentation` owns an already prepared `UnsignedReview` on the UI thread.
It shares the balance display's `ForegroundPresentation` queue and wakeup
mechanism. Producer notifications coalesce to one redraw signal; delivery reads
the original owner at the current C clock and arms one cancellable wakeup from
the returned remaining duration. Early or late timer callbacks only request a
new read. They cannot decide expiry, extend a deadline or authorize a signature.

Background, lock and replacement must close the presentation and clear its
views. Closed presentations drop source/receiver references and reject queued
or captured timer callbacks. A replacement owns a newly prepared review; no
review ID or snapshot is restored. The native single-review slot is released
by explicit cancellation or observed expiry, including failed rendering.

Delivery failures close the owner and invoke the unavailable callback even
when timer cancellation or owner cleanup throws. Renderer/fatal failures still
propagate, with cleanup problems preserved as suppressed exceptions. This also
tightens the existing balance presentation's failure cleanup. No exception is
treated as permission to show a previous snapshot. Activity integration remains
a separate acceptance slice; this adapter supplies no sending action.

`ReviewView` shows the selected network, all outputs in native order, exact
C-formatted input/output totals, fee and fee limit. It explicitly labels the
draft unsigned and its supplied funding unverified. It does not infer change,
available funds or ownership. Inspectable details retain the full draft ID and
outpoints, raw unsigned lock/expiry/index/sequence fields and unsigned byte size;
these do not establish current-chain finality. No countdown is cached in text.

Formatting bounds counts, numeric widths and hashes before publishing the full
new text. Previous text clears before any conversion that can fail. Hierarchy
save/restore is disabled, even for an older ordinary TextView state under the
same ID; detach clears the view. Autofill and content capture are excluded.
Actual foreground closure must still cancel the presentation and clear the view.

## Ordered continuation

1. Checked synthetic funding/prevout amounts, output classification, change
   ownership and review binding to exact transaction bytes. A server balance or
   history assertion cannot provide spending authority.
2. Independently qualify the exact original serialization and branch-specific
   signature-hash construction with offline public fixtures. No signing until
   scriptCode, input amount, branch/height, outputs and authorization are bound.
   The first standalone BLAKE2 reference candidate is
   [blocked for further security review](BLAKE2_REVIEW.md) and is not in builds;
   continue immutable review work independently of this candidate.
3. Synthetic signing and explicit review/cancellation, then qualified broadcast
   lifecycle and restart recovery. Real funds remain outside development tests.
4. Shielded wire/proof/witness/value/recovery qualification before exposing it.

TLS remains **BLOCKED — REQUIRES FURTHER SECURITY REVIEW** and excluded from
normal builds. Positive hardware custody still requires a qualified device.
