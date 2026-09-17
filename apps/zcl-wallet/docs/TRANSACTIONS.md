# Transparent transaction development

## Bounded Merkle-path consistency

The internal `zcl_merkle_branch_check` accepts a displayed transaction ID,
displayed expected root and a fixed-capacity branch with claimed uint32 count,
index and up to32 displayed sibling hashes, leaf level first. It requires
nonzero count, index<count, exact depth, final-node self-copy only at odd widths,
and unequal actual siblings along the path. Raw little-endian uint256 storage
bytes form each ordered64-byte SHA256d pair. Count halving avoids overflow even
at UINT32_MAX; work is bounded to64 hash calls, with no heap, I/O or retained
pointer. Complete work scratch clears after success and any entered failure.

Success proves only this hash path under the supplied shape/root. A header does
not separately commit the claimed count, and a path alone does not distinguish
a transaction leaf from an internal node or establish off-path uniqueness.
No block-size/height rule, consensus validity, accepted-chain inclusion,
freshness, maturity, unspentness, ownership, consent or signing authority is
established. No caller status is upgraded to authenticated funding. Header
inspection and exact source/outpoint/header composition remain separate work.

References are Git objects at original Zclassic `14a83d510ffd109d3fa09bf74ebf8c28854a263f`:
`src/primitives/block.cpp` (`GetMerkleBranch`, `CheckMerkleBranch`),
`src/merkleblock.cpp` (partial extraction) and `src/uint256.cpp` (`GetHex`).
Their SHA256 values are respectively
`87ca0c104fd6c6c03e6b64c30218ff65c723cd430cbfc05bcb4215a3ab8d2ea2`,
`d4018af26901efd1c4aef2c7498a99a08cf592e6fb804786440fef6cd9115f16` and
`c7b3388336fdc696c022f71d0ce1ce30a954b78586ebbaa0bb489b8bb4ea40f1`.
The original partial extractor also applies its200000-byte block constant /60
count guard; the helper above is a bounded hash primitive, not that network
message parser or a replacement consensus predicate. The full-block builder and
partial extractor have different mutation-detection scopes; this checker sees
only the supplied path. Original MIT notice is in `transaction-reference.LICENSE`.

`seed_merkle_vectors` in a `ZCL_ORACLE=ON` host build prints the exact committed
`native/tests/merkle_vectors.h` using OpenSSL. Inputs are public synthetic labels
and opaque subtree commitments, never real block/transaction validity fixtures.
The normal tests cover all leaves of full trees up to65 entries and uint32
boundaries; the oracle profile independently hashes through OpenSSL. Nine
mutations and every hash-provider failure position qualify exact shape and
retirement. Differential fuzzing and release-archive runtime tests pass; ARM64
remains build-only evidence.

## Single-call managed full-source preparation

`UnsignedReview.prepareFullSources` takes explicit funding selections and intended
outputs. JNI captures/copies each full source once under the existing process
review lock, parses bounded numeric fields and destination text, constructs the
current transparent unsigned wire and opens its review against those same owned
sources. An active review refuses before input preparation. No intermediate wire
is returned to Java, no source pointer survives, and caller arrays are never
wiped. Arrays/lists must remain stable throughout the synchronous worker call.

Current request descriptors, serialized wire and source metadata retire before
the bounded source allocation wipes/frees, including pending JNI exceptions and
dirty-provider failures. This shares fixed expiry, rollback, non-repeating IDs,
managed clock-failure cleanup and background-close requirements. Five JVM tests
and three preparation instrumentation tests per API30/35/36 qualify exact rows,
wire, refusal, ownership and lifetime. Foreground presentation and process-restart
qualification now cover both profiles on API30/35/36 x86_64. Source proofs and
signatures stay opaque; there is no chain/unspentness, consent or send authority.

## Explicit full-source draft construction

The internal `zcl_transaction_draft_full_sources` accepts the existing bounded
draft request with full v4 funding sources. Exact selected indexes and sequences
become inputs in request order; checked output destinations/amounts stay explicit.
Full-wire source identities are derived from the supplied bytes. Full-source
assessment then rechecks every outpoint, destination, total and fee ceiling
before the complete owned transaction is published. Failure preserves the caller's
output. Source views, assessment and candidate scratch retire on all work exits.

The original public builder remains narrow. No source pointer survives, no heap
is added, and no implicit change or coin selection occurs. Structurally valid
opaque proof/signature changes select a different unverified source identity;
this operation cannot establish inclusion, unspentness or consensus validity.
The managed preparation above constructs and opens over the same owned source
copy. No new signing, consent, custody or network authority is introduced.

## Managed full-source offline review

`UnsignedReview.openFullSources` explicitly opens an already constructed bounded
unsigned draft against1..8 full v4 funding sources. The JNI adapter captures each
array reference and length before allocation, bounds every source to102000 bytes
and the aggregate to816000, and allocates exactly the summed lengths. It copies
the draft and sources, releases all Java references before C preparation, then
clears draft/pointer metadata and wipes/frees the source allocation on every
exit. Pending exceptions remain pending; no JNI source/reference survives.

This shares the existing single process owner, non-repeating IDs, cancellation,
fixed expiry and background/clock-failure cleanup. `open` and `prepare` retain
their original source profile; no automatic fallback changes admission.
Callers must supply stable arrays during opening. The owner retains only public
checked review data and canonical current wire. These results establish no
source proof/signature validity, chain freshness, inclusion, unspentness,
custody, consent or signing/broadcast authority. There is no new send/UI action.
Full-source C draft construction and the explicit single-call managed
construction/opening adapter are available as described above.

Four real-JVM tests run with `-Xcheck:jni`. Three new plus five existing review
instrumentation tests pass on each x86_64 API30/35/36 emulator. Source replacement
during capture/allocation, partial JNI writes, non-null exceptional references,
maximum aggregate allocation and exact cleanup order have deterministic native
fixtures. Debug/release, minification and alignment gates pass; instrumentation
uses the debug target, and ARM64 is build-only evidence.

Public synthetic fixtures in `wallet-core/src/test/resources/full-review/`
reproduce with the `seed_full_review` target from a host `ZCL_FUZZ=ON` build,
run in a new empty directory. Compare its `draft`, `previous0` and `previous1`
against those resources. Opaque proofs/signatures are deliberately invalid;
these are never spendable or chain-validation fixtures. The two3326-byte
sources require one6652-byte native source allocation in the observed fixture.

## Owned full-source review lifecycle

The internal `zcl_review_open_full_sources` explicitly opens a review using
full-v4 source assessment. Preparation parses the bounded current transaction,
requires unsigned inputs, matches all sources and checks amounts/destinations,
then serializes the owned parsed value. Only complete preparation publishes
the next review ID. It retains no source pointer or source wire: callers can
retire every borrowed funding span immediately after return.

The existing owner, fixed deadline, rollback cancellation, non-repeating IDs,
snapshot/copy and background-clear rules apply. Legacy `zcl_review_open` keeps
its original source profile. The C operation adds no heap, chain evidence or
signing authority. Internal signing still requires its exact
per-use custody, consent and independently authenticated chain/unspentness
prerequisites; opaque source proofs/signatures remain unverified.

Full-source fixtures destroy borrowed data before subsequent reads, exercise
every source truncation, both networks/all conditional tails, replacement and
expiry boundaries, and verify unchanged owner/ID on failed preparation. The
existing complete review contract and dirty-provider retirement suite also run
through the new entry point. Differential lifetime/sighash fuzzing uses the
independent oracle. Managed ownership and real-VM qualification are described
above; explicit full-source draft construction remains separate.

## Explicit offline full-source assessment

`zcl_v4_source_prevout` matches the complete raw v4 source identity and selected
index before publishing its owned output. `zcl_v4_source_assess` then reuses the
existing assessment owner for destination templates, checked input/output totals,
explicit fee ceiling and exact current transaction identity. Every current
transaction predicate remains in force. At most eight stable borrowed sources
are inspected sequentially, each bounded to102000 bytes; there is no heap,
source retention or partial report, even after a later input/provider failure.

This explicit internal entry point produces offline data about supplied bytes.
It does not establish source consensus validity, inclusion, unspentness, maturity,
ownership, consent or current chain state. Opaque proofs/signatures are hashed,
not verified. Legacy assessment, draft, review and signing retain their existing
source profile; the standalone assessment grants no review ID. The path may inspect funding
containing shielded components, but cannot create a shielded spend.

The existing assessment contract also runs through this entry point. Additional
fixtures cover all conditional source tails, mixed source profiles, pinned
reference identities, signature-byte changes and legacy refusal isolation.
Dirty-provider tests cover both assessment modes and nested matching cleanup.

## Full v4 funding-source inspection foundation

The internal `zcl_v4_source_inspect` reads the complete canonical v4 layout,
including opaque Sapling descriptions, Groth JoinSplits and conditional
signatures. It returns full-wire SHA256d in displayed order, one owned
transparent output and raw metadata. Counts and scripts are bounded by
remaining wire within the original Zclassic 102,000-byte limit. Only the
selected output must fit the existing 25-byte script owner. Transparent values
and their sum remain bounded by MAX_MONEY. No heap, I/O or pointer is retained;
failures preserve the complete caller output.

This supplies structural inspection, not consensus validation, proof/signature
verification, trusted outpoint matching, inclusion, unspentness or signing
authority. Raw expiry and valueBalance do not establish valid accounting or
finality. Pinned original hash vectors need not be consensus-valid. Existing
legacy transparent-only prevout, draft and JNI source limits remain unchanged.
Wider source inspection is available only through the explicit full-source
assessment and opening operations above.
This does not implement shielded spending.

Four untouched reference wires and independent OpenSSL SHA256d identities
reproduce with `tools/project-original-sources.sh`; see
[COMPATIBILITY.md](COMPATIBILITY.md). Fixtures cover all conditional tails,
every truncation and single-byte change, minimal CompactSize, large vectors,
exact/over-limit wire and dirty hash-provider failures. Release-library
runtime fixtures pass API30/35/36; ARM64 is compile-only evidence.

## Reviewed output change ownership

The internal `zcl_review_output_change_check` checks one output from the same
live review against a consumed change index of the exact committed, recovered
wallet. It reuses the existing bounded wallet claim and ownership checks;
chain1 is mandatory. A receive address, P2SH destination, wrong wallet,
unconsumed index or mismatched output cannot pass as change. It neither reserves
another index nor modifies the wallet/journal. The original output amounts and
destinations remain part of the review; ownership does not authorize concealing
a payment or infer the user's intended output role.

Trusted monotonic time is sampled at admission and again after ownership work
and complete secret cleanup. Expiry or rollback invalidates the review, while
clock/provider errors refuse a match. OK refers only to this exact live ID and
row under the enclosing exclusive lock; it is not a retained approval token.
Per-use hardware authentication of the exact supplied record/header/entropy
remains an external prerequisite. Journal authentication does not prove recency
against filesystem rollback or recover historical indexes. There is no JNI/UI
entry point, consent, authenticated chain/unspentness or broadcast authority.
Any future display must recheck delayed delivery and retain exact output data.

Fixtures cover both networks/all entropy lengths, all16 output positions,
destroyed borrowed draft sources, wrong-wallet/receive/P2SH/index refusals,
entry/completion expiry/rollback/clock errors and unchanged journals. Existing
provider-fault observations now also prove copied claims and complete secret
cleanup before the final change-check sample. The differential claim fuzzer
models this output match and its two clock samples alongside input signing.

## Complete internal wallet signing

`zcl_review_wallet_transaction_sign` composes the review-bound input signer
and completion-time wire publisher. It admits exactly one wallet claim per
reviewed input, copies the bounded claim metadata and candidate, checks every
input's ownership before the first ECDSA operation, then signs each input into
invocation-owned staging. After all inputs succeed it retires the copied claim
pointers and independently verifies/assembles the complete signed wire. A later
input or completion failure cannot publish a partial signature or transaction;
caller bytes and length remain unchanged. Staging clears on every work exit.

The operation supports the existing 1..8-input P2PKH profile and samples the
trusted monotonic clock at most `4*input_count+3` times. Each input signer still
checks time before admission, before signing and after public verification;
the final wire publisher checks again after serialization. There is no retry,
new allocation, key export, storage write, change reservation or review
consumption. Stable borrowed payloads may share identical wallet bytes and
remain caller-owned; the caller must clear its entropy after return.

This is an internal C foundation with no JNI/UI entry point. Per-use hardware
custody, consent for the exact review, authenticated chain/unspentness and
freshness at delayed delivery/broadcast remain required external prerequisites.
Host sanitizer/oracle, provider-fault, mutation and fuzz fixtures qualify the
composition. Release-library x86_64 fixtures pass API30/35/36; ARM64 builds only.

## Review-bound wallet signing foundation

The internal `zcl_review_input_wallet_sign` operation can produce one detached
public signature only for an input already owned by the same live review. It
does not accept a digest, script, amount or arbitrary derivation path from its
caller. The C core recomputes the contextual ZIP-243 digest from that review,
requires the exact committed wallet and recovered receive0 or consumed-change
address to match the assessed input, derives the corresponding existing BIP44
private key, signs, then runs the strict DER/low-S/public-key/HASH160 verifier.

A trusted local monotonic clock is sampled before admission, after key
derivation and before ECDSA, and after public verification. Rollback or expiry
invalidates the same review under existing rules; any clock, ownership, context,
provider or verification refusal preserves all caller output bytes. Secret
objects retire at their last uses and the whole invocation work object clears.

This internal composition is qualified with synthetic host fixtures and native
x86_64 execution on API30/35/36 against release-built libraries. ARM64 compilation
does not qualify physical-device execution. There is no
JNI or send-screen entry point. The Android adapter must first establish per-use
hardware-backed custody of the exact wallet, explicit consent for the exact
review, and independently authenticated current chain/unspentness. It must also
recheck delayed delivery and broadcast freshness. A returned signature is not a
reusable approval, does not consume the review or reserve change, and grants no
broadcast authority by itself.

## Signed-wire completion boundary

The internal `zcl_review_p2pkh_complete` operation composes the existing public
signature verification and signed-wire assembly with a trusted local monotonic
clock. It samples before admission, stages all signed bytes privately, then
samples again after verification/serialization and checks the same review ID
before publishing. Expiry at the exact deadline or clock rollback clears the
review under its existing rules. Clock/provider failure leaves caller bytes and
length unchanged. The staged wire clears on every entered exit. The existing
fixed-timestamp assembler retains its narrower documented contract.

The clock callback runs synchronously under the caller's existing exclusive
review lock. It must provide local elapsed time, retain no span and make no
reentrant wallet call. A server timestamp or wall clock cannot supply it.
Success observes liveness at the last sample; it does not impose a scheduler
or provider interruption deadline, consume the review, prove wallet custody,
authenticate chain context, or grant consent/broadcast authority. The eventual
adapter must still recheck at delayed delivery and broadcast. There is no JNI
or send-screen entry point for this operation.

Deterministic fixtures cover both networks, 1..8 inputs, 1/16 outputs, exact
signed-byte equivalence, capacity boundaries, entry/completion expiry, rollback,
clock refusal and missing samples. A separate source-copy fixture observes
private staging, malformed provider lengths and full cleanup. Three mutations
remove the second clock sample, final review check or wipe; all fail. The signed
wire differential fuzzer also models completion time independently. Host
sanitizers/oracles and x86_64 execution against release-built Android archives
qualify this scope; ARM64 compilation does not establish device behavior.

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

The separate host `seed_sighash_vectors` now matches all 130 untouched v4
signature-hash results in that pinned original dataset before generating any
new expected values. It uses its own bounded byte reader and libsodium, with
no wallet parser, serializer, BLAKE2 provider, JNI, original C++ compilation or
node execution. Original `interpreter.cpp` SHA256 is
`d13b7b9590e5ae8d992d731f1436f9f60e1c21ed21b45b7e09e4dbbc668c35c8`.
The original fixture calls SignatureHash with amount zero. Its expected uint256
display is reversed for comparison with raw digest bytes.

Generation also requires both published ZIP 243 transparent-input records to
match, covering independently specified nonzero 64-bit amounts with NONE and
SINGLE flags. Their raw digest byte order is compared directly. The selected
records, pinned source identity, complete license and extraction recipe are in
[zip243-reference.md](../native/tests/zip243-reference.md). No original expected
hash or derived digest changes: this closes the amount-evidence gap before
using the fixtures to qualify wallet code. Three mutants reverse, truncate or
zero the amount; each passes all original zero-amount cases and fails this gate.

This oracle reads only the bounded shapes present in the reference data:
v4 header/group, at most 8 inputs/16 outputs, short canonical lengths, up to
4 spends/4 shielded outputs/3 Groth JoinSplits and exact trailing signatures.
Spend authorization signatures, JoinSplit signatures and the binding signature
are consumed but excluded from their specified component hashes. Proof bytes
are opaque. No proof, script, funding, branch-at-height or chain-validity claim
comes from this byte comparison. It does not add shielded wallet support.

Observed original cases include ALL-like, NONE and SINGLE base behavior,
ANYONECANPAY with ALL-like/NONE, four explicit branch values and nine empty
scriptCode spans. SINGLE+ANYONECANPAY and absent matching SINGLE output are
not covered by that dataset. The generator does not infer current branch
selection from these historical values.

After all 130 original and two ZIP comparisons succeed, the generator projects only the
same rows 203/208/296 described above. It preserves their exact transparent
prefixes and replaces the shielded tail with eleven zero bytes. The resulting
144 SIGHASH_ALL cases cover every selected input, four explicit branch values,
amounts zero/one/max-money, original opaque scriptCode and a public P2PKH script
whose hash bytes are 0..19. Digests are raw 32-byte values in
`native/tests/sighash_vectors.h`; these are derived expected values, not
untouched original transactions. The bounded constructor's separate
qualification is described below.

From `apps/zcl-wallet`, build the host oracle and create a new evidence directory:

```sh
cmake -S native -B native/build/sighash-oracle -DCMAKE_C_COMPILER=clang-20 \
  -DZCL_SANITIZE=ON -DZCL_ORACLE=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build native/build/sighash-oracle --target seed_sighash_vectors -j4
bash tools/project-original-sighashes.sh /path/to/original-checkout \
  native/build/sighash-oracle/seed_sighash_vectors /path/to/new-report
cmp native/tests/sighash_vectors.h /path/to/new-report/sighash-vectors.h
```

The script reads pinned Git objects, checks the original dataset hash and
records source/binary/output identities. The C oracle bounds lines, counts,
spans and row order, checks every provider/IO result and emits expected values
only after complete original verification. An existing report directory is
refused with its bytes preserved. Ten malformed-input cases and eight oracle
mutants fail before emitting expected bytes. Clang/GCC analysis, strict C17
compilation and ASan/UBSan/LSan verification pass. Evidence is retained under
`.cache/android-wallet/sighash-oracle-20260914/`. Android production is unchanged;
the bounded constructor now has separate qualification below. Authenticated
authorization and current branch/height selection remain open.

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

Opening clears its staged review on success and every preparation failure.
The separate preparation frame clears its owned parsed transaction before
returning, and snapshot publication clears its temporary copy after transfer.
A source-copy regression injects dirty partial failures at parsing, assessment
and serialization, observes actual full-object erasure, and verifies unchanged
owner/ID outputs. Borrowed-wire mutation after parsing cannot alter the exact
published draft. These operations contain public transaction fields, not keys;
this bounded scratch-retirement evidence does not erase every downstream copy
or grant consent, chain trust or signing authority.

The JNI adapter retires its copied snapshot before VM allocation and erases
the full numeric/wire response scratch after attempted publication, including
allocation and pending-exception failures. The returned VM array is a separate
copy. Failure cancellation remains bound to the original review ID, so a
concurrent replacement is preserved. The source-only fault/fuzz fixture checks
these lifetimes without introducing any production hook or managed-erasure claim.

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
original signature hashes are separate gates. Review Activity acceptance uses
only a nonexported debug fixture host; no wallet sending route is wired yet;
the review cannot enable the quarantined transport or transaction signing.

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
API35 emulator transfer tests qualify cancellation, expiry and owned copies;
Activity lifecycle has a separate public fixture acceptance below. Neither
establishes original-node transaction acceptance.

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
treated as permission to show a previous snapshot. This adapter supplies no
sending action.

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

The nonexported debug `WalletReviewFixtureActivity` displays only explicitly
transferred public review owners prepared by instrumentation off the UI thread.
It has FLAG_SECURE, one obscured-touch-filtered close action, no wallet/Keystore
access and no intent/Bundle/asset replay. An owner arriving after foreground loss
is immediately cancelled. Pause/destruction clear the presentation reference
before closing it and clear the view in finally. Resume/recreation starts empty.

Device fixtures observe repeated recreation, old-view clearing, late preparation
after backgrounding, queued redraw cancellation and close/new-Activity behavior.
The package gate requires each debug fixture host to be explicitly nonexported
and absent from the release manifest. Independent export, missing-export,
missing-host and release-host mutations exercise both host boundaries. Public
transaction fixtures remain confined to the test APK.

`tools/check-process-relaunch.sh <adb> <emulator-serial> <new-report-directory>
review` runs the separate opt-in process acceptance. It verifies a live public
draft on screen, receives readiness/profile/PID from that fixture and matches
the current process before force-stop. The new process must have a different
PID, an empty review view and an available native review slot. Explicitly
preparing a new public draft does not automatically display it. Invalid profiles
or non-emulator serials refuse before creating a report directory or calling adb.
Preparation is intentionally terminated; its instrumentation crash message is
expected evidence of process loss, not a parser/sanitizer finding.

The `review-full` controller profile constructs and opens over full sources in
one JNI call, then runs the same process acceptance. It requires matching
`review-full` readiness before termination. Both relaunch profiles use the
strict instrumentation result checker, including skip and incomplete-run refusal.
For the display/render/lifecycle suites, pass `-e reviewSourceProfile full` to
select that same public preparation; absence or `narrow` retains the original
route, and unknown values refuse. Both profiles pass14 tests and both process
acceptances on API30/35/36 x86_64. The original API36 software-emulation timeout
is retained in evidence; a fresh isolated accelerated API36 passes unchanged
assertions and timeouts. These fixtures add no production route or authority.

## Unsigned draft construction

`zcl_transaction_draft` takes a fixed request with up to8 explicit funding
selections and16 destination/value outputs, selected network, raw lock/expiry/
sequence fields and an absolute fee ceiling. It derives each outpoint ID from
the supplied canonical previous bytes, checks the selected index and builds
each exact standard destination script. Input/output order is preserved, and
all current input scripts and unused rows start empty.

The existing complete assessment must pass before the owned transaction is
published. Duplicate outpoints, unsupported funding scripts, wrong-network
destinations, excessive amounts, underfunding and excessive fees refuse without
changing the caller result. Different outputs may share a previous transaction.
Caller request/source bytes stay stable for the synchronous call; no pointer,
heap allocation or secret is retained. The result is independent of those bytes.

This constructs a bounded unsigned v4 transparent candidate. It does not select
coins, reserve/classify change, prove source inclusion/unspentness/maturity,
qualify current-chain finality, authenticate ownership or sign/broadcast. Previous
transactions remain limited to the existing canonical codec subset; legacy,
v3, shielded or larger sources are unsupported.

`UnsignedReview.prepare` is the thin managed factory for this constructor. Each
funding row supplies previous bytes, output index and sequence; each output
supplies an immutable canonical address and amount. Lists and bytes must remain
stable during the synchronous call. The factory checks counts before bounded
allocation and keeps private copies of every previous transaction through both
C construction and C review opening. Caller mutation during the intervening
clock callback cannot substitute funding. Temporary source/draft byte copies
clear in finally, including BUSY, malformed input and clock failures.

The stateless JNI constructor accepts1..8 previous byte arrays of<=1925bytes,
1..16 canonical address byte arrays of<=35bytes and exactly
`3 + 2*input_count + output_count` signed longs, capped at35. The parameters are
lock time, expiry height, absolute fee ceiling, each input's index/sequence pair,
then each output's amount. C checks nonnegative uint32/money ranges before
conversion. Addresses must parse on the explicitly selected network; network
identity cannot be inferred or changed from a bare public hash.

One checked16272-byte host allocation owns all native source copies until
construction returns. Each local element reference releases after its copy.
Partial VM reads and pending exceptions stop processing; every cleanup clears
the whole allocation before freeing once. Java publication occurs afterward
and returns one status byte followed only on success by canonical unsigned
bytes. A construction/publication failure cannot touch another review owner.
Opening still uses the existing single-slot lifetime and can return BUSY.

Fake VM fault tests check partial reads, every small-fixture read ordinal,
allocation/publication failure, NULL/oversized arrays, signed extremes and
maximum8-input/16-output requests. Real JVM and emulator tests bind the prepared
bytes to the exact public draft fixture on both networks. The actual debug
review Activity lifecycle fixture now prepares its drafts through this factory;
background/recreation/close continue to cancel the same C owner.

## Bounded SIGHASH_ALL construction

`native/src/transaction_sighash.h` exposes one internal public-data operation,
`zcl_transaction_sighash_all`. It uses the existing complete transparent v4
object validator and the reviewed bounded BLAKE2b-256 helper. It has no JNI,
key, signing, persistence, endpoint or send-screen caller.

The caller supplies an existing input index, exact scriptCode bytes (0..128),
input amount (0..MAX_MONEY) and explicit uint32 branch value. Only SIGHASH_ALL
without ANYONECANPAY is constructed. Other hash modes, NOT_AN_INPUT, legacy,
v3, shielded and larger wallet profiles remain outside this API. Branch values
are data for domain separation; this function does not select or authenticate
the current chain branch. ScriptCode is supplied explicitly, without inferring
a previous or redeem script. A successful hash does not prove ownership,
script validity, funding/unspentness, maturity, review or user consent.

The method hashes all outpoints, sequences and outputs under their exact
original domains, then constructs the v4 final preimage with three zero
shielded/JoinSplit component hashes, exact lock/expiry/valueBalance/type and
the selected outpoint/scriptCode/amount/sequence. It reverses displayed
outpoint IDs back to wire order and emits raw digest bytes, unlike the reversed
display order of transaction IDs. Input scriptSig bytes are excluded by v4;
changing them can change a transaction ID without changing this signature
hash. Future signing must still bind the exact reviewed unsigned bytes.

Every field is checked before iteration/copy. A fixed 544-byte writer covers
the largest output component; the final preimage is at most 397 bytes. The
800-byte work object on measured 64-bit builds owns all component hashes,
the final domain and digest. Every provider result is checked; later hash
steps stop after failure, output remains untouched and the work object clears.
Small public endian/outpoint helper temporaries and provider compression
temporaries receive no secret-erasure claim. The interface accepts no secrets.
There is no allocation, retained pointer, mutable production global, timer,
thread or I/O. Callers must own stable nonoverlapping objects for the call.

All 144 independent expected digests pass at every output capacity 0..64.
Maximum 8-input/16-output, 128-byte scriptCode and 25-byte output-script cases
pass, including unchanged digests after changing every scriptSig. The optional
host oracle independently parses serialized fixtures with libsodium and
compares their values. Invalid spans/objects preserve output and input bytes.
Each of the four hash-step faults dirties private output before refusal; live
cleanup observations prove that work spans clear without post-lifetime reads.
Eleven mutants alter branch, amount, sequence, outpoint byte order, scriptSig
inclusion, output domain, empty component bytes, failure propagation,
publication or full/partial cleanup; each fails its intended assertion.

Bounded malformed-object/preimage fuzzing with the independent oracle completes
1,116,954 executions in 121 seconds without a finding, including explicit
maximum-profile seeds. The status comparison reuses the wallet's existing
validator/serializer; digest comparison uses the separate reader/libsodium.
This does not claim independent transaction-validity or proof verification.
The generator still matches all 130 original plus two published nonzero-amount
records, and its expected header reproduces byte-for-byte after adding its
shared host oracle mode. All 67 native ASan/UBSan/LSan groups pass in 45.23
seconds. Clang/GCC and unchanged production/test complexity caps pass.

NDK ARM64/x86-64 builds and Android/JVM/lint/artifact gates pass. The final test
executable links the actual Android release archives; all 144 vectors, maximum
profiles, capacities and refusals pass on x86-64 API30/35/36. Its exact SHA256 is
`6e45587aaf4b4294acc47293c150dcb801f193137022d5ce36a6e2f985ee9517`.
ARM64 is compiled only. Standalone ELF load alignment is 16KiB, with RELRO,
immediate binding and non-executable stack. Debug/test/unsigned release APKs
remain byte-identical because this internal operation is not pulled into JNI.
These are standalone public C observations, not a new APK sending feature.

Evidence is retained in `.cache/android-wallet/sighash-core-20260914/`.
For focused host checks, use `ctest --test-dir native/build/safety-active
-R '^wallet_sighash' --output-on-failure`; the optional `ZCL_ORACLE=ON` profile
adds the independent reader/libsodium comparison. A separate Clang build with
`ZCL_FUZZ=ON`, `ZCL_SANITIZE=ON`, `ZCL_ORACLE=ON` provides `fuzz_sighash`;
use private corpus/artifact directories, input max4096, timeout5, RSS512MiB and
a bounded duration. Normal production and Android builds never need libsodium.

## Digest binding to a live unsigned review

The internal `zcl_review_sighash_p2pkh` operation now resolves input amount and
scriptCode exclusively from the review's owned assessment, which already
matched each exact previous transaction hash/index. It reconstructs the exact
standard P2PKH script and parses only the review's owned canonical unsigned
wire. Its arguments contain no replacement transaction, amount, script or
previous-source pointer. P2SH selected inputs refuse because their redeem
scripts require a separate ownership and script contract. Other inputs and
all outputs remain bound by SIGHASH_ALL.

This is a public-data operation with no JNI or signing caller. The caller's
explicit branch value remains data, without current-chain/height authority.
The digest itself does not bind network, fee policy or approval: those remain
properties of the exact live review and future authenticated authorization.
Computing or retaining a digest cannot extend consent across callbacks,
backgrounding, cancellation or replacement. No key or signature is introduced.

The existing lifetime transition is shared unchanged with snapshot/wire reads.
An exact live ID is required; stale IDs cannot mutate a replacement. Inclusive
expiry and monotonic-clock rollback clear retained review bytes. Valid live
calls advance the observed clock even if capacity/index/script kind refuses,
and never extend the fixed deadline. NULL owner/output refuses before a state
transition. All access still requires the adapter's same serialized lock.

The invocation owns one fixed 2272-byte public work object on measured 64-bit
builds; its optimized host frame is 2328 bytes, not a whole-call-chain bound.
It checks parsing, script construction and digest statuses in order, publishes
only 32 bytes on complete success, then clears the whole object on every work
exit. Early refusals skip parsing and hashing. There is no heap, secret, retained
pointer, global production state, I/O or new worker. Provider and parser frames
remain separately bounded by the unchanged native build gates.

Unit checks cover both networks, four explicit branch values, all 65 output
capacities, amount 0/1/MAX_MONEY, maximum 8-input/16-output drafts, every selected
maximum input, P2SH/index/NULL refusals, immutable copied/source bytes, exact
expiry, rollback, cancellation and stale IDs. The optional independent
reader/libsodium oracle matches the expected digests. Three dirty stage faults
on two distinct P2PKH inputs prove stopped work, unchanged output/owner and
cleanup observed during live object lifetime. Thirteen mutations of funding
row, index, branch, script source, P2SH handling, liveness, parse/script failure,
publication, full/partial cleanup, deadline extension and stale-ID checking are
all caught by their intended assertions.

The existing review state-machine fuzzer now includes hash calls under its
independent lifetime model, with optional independent digest comparison.
It completes 296,803 executions in 121 seconds without a finding. This models
serialized calls, not concurrent adapter-lock correctness. Final Clang/GCC
analysis and all 69 native ASan/UBSan/LSan tests pass in 45.92 seconds, with
unchanged 10/15 complexity caps (469 production/932 test functions). A new fault
observer bound is checked explicitly before iterating its four captured spans;
the initial GCC fixture finding and corrected evidence are both preserved.

NDK ARM64/x86-64 builds, JVM tests, Android lint, APK alignment/fixture isolation
and architecture gates pass. Unlike the previous hash-only additions, extracting
the shared review lifetime helper changes APK bytes. Fresh debug installations
therefore exercise all five real-JNI unsigned-review and four Activity-lifecycle
cases on API 30/35/36. API 30 completes the combined nine in 67.921 seconds.
The initial API 35/36 combined clients hit their 90-second host limit; retained
Android logs subsequently record all nine completing with zero failures and a
disconnected instrumentation watcher. Process inspection confirms teardown.
Separate complete reports then pass all five native-review cases in 5.326/3.659
seconds and all four lifecycle cases in 132.196/114.592 seconds on API 35/36.
The bounded retry allowance is 180 seconds per group, with every original
fixture assertion and timeout unchanged.

The final standalone C tests link the actual release archives and pass on
x86-64 API 30/35/36, including all capacities, maximum profiles and lifetime
refusals. Their exact executable SHA256 is
`d921a37ba9bb2c18bdd2e6270fbb6a14285f31c2544d92825d6bfeddeddef2ab`.
ARM64 is compiled only; both standalone ELFs have 16KiB load alignment, RELRO,
immediate binding and a non-executable stack. The shell-owned fixtures process
public bytes without opening app/Keystore/wallet storage.

The locally signed minified APK has SHA256
`3ffb40ee16debf3c0f871f7e790fc05deae7ac673fc5f54fdde25b1c42424af3`;
all unsigned archive entries compare byte-identically after signing. It passes
the complete API 30 camera denial/grant/public-QR/review/worker-cleanup fixture
in 25.735 seconds. API 30 and36 retain this minified build; API35 retains debug.
These checks do not expose the new internal digest as a JNI or sending feature,
and do not qualify hardware custody or current-chain authentication.

Evidence is retained in `.cache/android-wallet/review-sighash-20260914/`.
Focused checks use `ctest --test-dir native/build/safety-active
-R '^wallet_review_sighash' --output-on-failure`. `ZCL_ORACLE=ON` adds host-only
independent comparisons; `ZCL_FUZZ=ON` provides the extended `fuzz_review` with
max_len 640, timeout 5, RSS 512MiB and a bounded duration. Android builds use
neither libsodium nor an oracle executable.

## Input key ownership within a live review — 2026-09-14

The internal `zcl_review_input_wallet_check` compares one exact reviewed P2PKH
input with a recovered wallet key location. The supported v1 locations are the
existing receive key at external index0 and previously consumed change indexes
0..65534. It requires the exact committed wallet record and matching network.
Receive0 works without a change journal but refuses a pending wallet. Change
uses the authenticated head/position and recovered derivation qualified in
[CHANGE_STORAGE.md](CHANGE_STORAGE.md); it never reserves another index.

The caller must first authenticate the exact wallet record/header/entropy using
GCM and the platform's per-use hardware policy. C cannot establish that platform
prerequisite. The claim carries bounded spans and a candidate key location; it
is not an ownership assertion. The wrapper copies its scalars, path, record and
entropy before storage/provider work, encodes only the selected owned assessment
row, and compares the derived address. Every entered private-work path clears
the whole work, including its entropy copy. The caller retains responsibility
for its own entropy. No key, pointer, digest, reusable approval token or JNI
entry is returned or added.

Calls require the same exclusive adapter lock and stable, nonoverlapping spans
as other review operations. Stale ID, inclusive expiry and clock rollback use
the shared lifetime transition. A live failed check advances the observed clock
without extending the fixed deadline. Success describes this input at the
supplied time, not completion-time freshness, user consent, other inputs,
funding/unspentness or current branch/height. Storage can block; use a worker.
A future signer must recheck the exact review and authenticated context within
its own operation and cannot cache this result as authority. The existing store
opener can establish its private directory/lock and syncs the parent; no wallet
or journal bytes are changed by this comparison.

Real-provider tests cover both networks and all five entropy widths; correct,
unused and wrong consumed keys; the maximum eight reviewed inputs after their
original borrowed sources are destroyed; receive-only and pending wallets;
altered ciphertext/entropy, every corrupted head byte, truncated state, NULL
and size/index bounds, P2SH refusal and review lifetime transitions. Separate
dirty-provider tests cover preparation, encoding, bogus lengths, storage,
pending selection, RNG and derivation failures. They mutate every caller span
and the claim metadata after copying, inspect private entropy and blinding
clearing while those objects remain live, and verify the owner stays unchanged
at the same clock. Fifteen deliberately broken variants fail their intended
assertions, including ignored errors, stale-source reuse and omitted wipes.

All 74 native ASan/UBSan/LSan groups pass in 55.23 seconds. Clang/GCC production
and new fixture analysis pass, with unchanged complexity caps10/15 (479/1001
functions). The measured optimized host frame is1608 bytes; that is per-frame
evidence rather than a whole call-chain stack measurement. The new bounded
fuzzer completes21,928 executions in121 seconds without a finding. It models
the review clock independently, mutates bounded wallet/secret/journal claims,
and checks complete wallet/journal byte and size preservation. Every case owns
and removes its own synthetic directory; fuzzed bytes never choose a path.
The registered fuzz regression exercises110 profile/lifetime cases.

NDK ARM64/x86-64, JVM, Android lint, APK alignment/fixture isolation and
architecture gates pass. This internal operation is unused by JNI, so the
rebuilt debug/unsigned-release/test APK bytes remain identical to the consumed
change milestone. Existing installed APK evidence is not relabeled as a fresh
device run. Standalone C qualification links the actual new release archives
and passes on x86-64 API30/35/36. Its executable SHA256 is
`3c410fb79d1f4b1ee7ebb9f27d37c24c5e8b9e09d0dde3293332a3b352971375`.
ARM64 is compiled only, with executable SHA256
`ce8660d59eedcce175a95c1f742e469278ba5e66c0f4846326f0cd54bc65a703`.
Both ELFs have16KiB load alignment, RELRO, immediate binding and non-executable
stacks. Exact device results and source/artifact hashes are retained in
`.cache/android-wallet/review-ownership-20260914/`. The test uses published
entropy and inert ciphertext in shell-owned isolated directories and provides
no GCM/hardware-custody qualification.

Focused checks: `ctest --test-dir native/build/safety-active -R
'^wallet_review_wallet' --output-on-failure`. With `ZCL_FUZZ=ON`, run
`fuzz_review_wallet` with max_len252, timeout5, RSS512MiB and a bounded duration.

## Candidate branch, expiry and finality checks — 2026-09-14

The internal `zcl_review_sighash_context` selects a v4 digest branch from an
explicit candidate block and checks the exact owned review's expiry and finality
fields first. It does not authenticate a current chain or authorize signing.
Its candidate contains network, block height and an explicitly selected
lock-time comparison cutoff. Height is already the candidate block's height,
not a tip to increment silently. Supported height/time domains are0..INT32_MAX
and0..INT64_MAX. The enclosing chain adapter must independently establish the
source, currentness and appropriate block/relay time cutoff; server assertions
and the Android wall clock are not chain evidence.

`zcl_transaction_v4_branch` implements the pinned original schedule. Pre-Sapling
heights refuse because this wallet codec supports only v4. Original Overwinter
and Sapling activate together at476969 on mainnet and20 on testnet. Bubbles
changes the branch at585318/6350. Bubbly activates at585322 on mainnet and is
disabled on testnet; Buttercup at707000/78856 retains the same `0x930b540d`
branch ID. No modern Zcash upgrade is substituted. The lookup describes the
pinned source profile, not whether an external current-height claim is true.

The context wrapper matches the candidate and reviewed networks, selects the
branch, rejects a nonzero expiry below the candidate height, and applies the
original `IsFinalTx` comparisons. Expiry equality passes, and zero expiry retains
its wire meaning. Lock0 passes. A lock below500000000 compares with height;
otherwise it compares with the supplied time cutoff. The comparison is strictly
less-than. If it does not pass, every input sequence must be UINT32_MAX. The
scan includes all eight possible inputs, independently of which input is being
hashed. Expired, nonfinal and out-of-domain candidates return OUT_OF_RANGE.
No expiry value, sequence, lock time or transaction byte is silently rewritten.

The candidate is copied before any provider call. Shared review ID, inclusive
expiry and rollback rules remain in force. After contextual checks, the existing
P2PKH digest resolves exact wire/script/amount from that same owned review.
Only32 digest bytes publish on complete success, and the entire private candidate
clears on every entered exit. The result grants no ownership, user consent,
unspentness/maturity or broadcast authority. This is not complete contextual
validation or mempool policy; for example, it does not implement the original
three-block expiring-soon relay policy or choose an expiry horizon. A future
signer must independently bind authenticated context/consent and recheck its
operation lifetime rather than caching this digest as authorization.

`tools/project-original-context.sh <original-checkout> <new-directory>` extracts
all seven epoch IDs and both complete activation schedules from exact Git
objects at `14a83d510ffd109d3fa09bf74ebf8c28854a263f`. It verifies SHA256 of
`chainparams.cpp`, `consensus/upgrades.cpp`, `consensus/params.h` and `main.cpp`
before parsing; output uses a new private directory with a10-second CPU limit.
A second projection reproduces the committed reference header/checksum exactly;
an existing output directory refuses and its objects remain intact. The generated
reference SHA256 is
`46c9ae0fb6daf3300d6940c48b9c74f86046f578e3995d2c98e2d310ed83c918`.
Normal safety checks verify that checksum. Tests independently traverse every
projected epoch and compare1,600,002 consecutive network/height pairs, plus
integer-domain edges. This projects source data and inspects original semantics;
it does not execute the original C++ or claim original-node acceptance.

Review tests cover every activation boundary on both networks, all65 capacities,
destroyed borrowed sources, zero/equal/past expiry, height/time lock threshold
and equality,64-bit time, all-eight-input finality, P2SH/index/NULL/network
refusals and lifetime transitions. Oracle mode independently compares selected
digests with the qualified reader/libsodium. Dirty-provider faults mutate the
caller candidate after copying and return plausible branch/digest data with
failure; output preservation and live private-work clearing still hold.
All19 deliberate mutants fail intended assertions. The existing review fuzzer
now models eight operations, including context-based hashing, and compares
arbitrary-height branch lookup with the projected epoch traversal. It completes
327,566 executions in121 seconds without a finding, with max_len640, timeout5
and RSS512MiB. Its previous campaign artifacts remain separate.

All77 native ASan/UBSan/LSan groups pass in54.98 seconds. Clang/GCC analysis,
including new fault/oracle fixtures and both extended fuzzer modes, passes with
unchanged10/15 complexity caps (484/1027 functions). Optimized host frame
measurements are0 bytes for the scalar lookup and120 for contextual hashing;
these do not measure the complete nested hash call chain.

NDK ARM64/x86-64, JVM, Android lint, APK alignment/fixture isolation and
architecture gates pass. APKs remain byte-identical because the new internal
operations have no JNI caller. Separate tests linked to the actual new release
archives pass on x86-64 API30/35/36. Executable SHA256 values are
`d5520a22c4755356ef7af995a172485c766f430e5ec35b10af445c09ee0e68e0`
(branch traversal) and
`b5a831c0c6d6393f3bd8848516642afbb56eb39b747043c73735f833cda11147`
(review context). ARM64 is compiled only. All four standalone ELFs have16KiB
alignment, RELRO/NOW and non-executable stacks. They process public fixtures
without app wallet, Keystore, endpoint or node access. Evidence and exact
source/archive/artifact hashes are retained in
`.cache/android-wallet/review-context-20260914/`.

Focused checks use `ctest --test-dir native/build/safety-active -R
'^wallet_(transaction_context|review_context)' --output-on-failure`.
`ZCL_ORACLE=ON` adds independent digest comparisons; `ZCL_FUZZ=ON` builds the
extended `fuzz_review`. Neither oracle dependency is included in Android.

## Bounded synthetic ECDSA primitive — 2026-09-14

Internal `zcl_signature_create` takes exactly32 secret and32 already computed
digest bytes. It returns only public low-S DER and a compressed33-byte public
key, with zero unused bytes/padding. This operation has no JNI, wallet unlock,
script pushes, sighash-type byte or transaction publication. Its current
callers are isolated synthetic tests. A future wallet signer must bind
platform-authenticated wallet/key ownership, current chain context, the exact
live review and explicit consent within its own operation, then recheck
cancellation/completion before publishing. A raw signature is not authorization.

The primitive copies both inputs before OS randomness or provider work, obtains
fresh32-byte OS blinding, and uses the existing checked1..1024-byte transient
secp256k1 context. It uses the pinned RFC6979/HMAC-SHA256 nonce function with at
most eight candidates and no fallback or key retry. The cap is wallet resource
policy; normal signatures match the provider's default deterministic output
exactly. It checks every provider result, rejects non-low-S output, serializes,
parses both public encodings, compares the parsed key and verifies the exact
digest before publication. Only complete success copies the entire output.
Every entered path releases/clears the context and clears the whole private
work, including secret, digest and blinding copies. The caller must clear its
own secret. Inputs remain caller-owned and stable during the synchronous call.

Tests cover64 scalar/digest profiles with repeated fresh blinding, the known
generator, zero/order/maximum scalar refusals, order-minus-one success, message
reduction at the order, strict minimal DER/low-S, changed-digest rejection,
NULL/length/SIZE_MAX bounds, guards and whole-output failure preservation.
Optional host-only OpenSSL3 independently derives the public key, parses and
re-encodes strict DER, checks low-S, and verifies the supplied digest through
EVP_PKEY_verify. It does not rehash the digest or replace Zclassic's BLAKE2
signature-hash domain. No OpenSSL dependency enters Android.

Linux fault fixtures retain real context allocation while substituting signing
providers. They cover every stage, dirty error outputs, oversized returned
lengths, RNG/OOM/context failures, mutation of both caller inputs after copying,
all eight allowed nonce attempts, cap/UINT_MAX/algorithm/data refusals, and live
nonce/secret/context clearing with exactly one release. The24 deliberate
mutants are all detected:23 by intended fixture assertions, and the skipped
context-error mutant by UBSan at its invalid nonnull provider call. No compiler
or sanitizer failure is counted as a successful build. The original mutation
runner expected every defect to reach an assertion; its preserved initial log
instead shows UBSan correctly intercepting that call earlier.

The new OpenSSL-enabled fuzzer completes36,242 executions in121 seconds without
a finding, with max_len66, timeout5 and RSS512MiB (observed peak267MiB). It checks
output guards/preservation, scalar validity, public signature verification and
determinism under fresh OS blinding. All79 native ASan/UBSan/LSan groups pass
in55.70 seconds. Clang/GCC production/fixture/oracle analysis passes; complexity
caps remain10/15 (490/1066 functions). Optimized host frames are552 bytes for
the entry and8 for its nonce callback; these are not whole-call-chain totals.

GCC initially rejected the test provider's NULL assertions because its function
definitions inherited caller-side nonnull annotations. That fixture translation
unit now uses the provider's own SECP256K1_BUILD mode, preserving runtime NULL
assertions while production callers retain their normal annotations. No warning
or security check is suppressed. Initial/final logs remain available.

NDK ARM64/x86-64, JVM, Android lint, APK alignment/fixture isolation and
architecture checks pass. APK bytes remain unchanged because this primitive
has no JNI caller. A standalone test linked against the actual new release
archives passes on x86-64 API30/35/36; its SHA256 is
`0f2c40db0e05547cd3e00f0897b3d59877aa0fcdcd0490f2a71e165d80f32ce2`.
ARM64 is compiled only (SHA256
`2cef86867587df2b78a4bcb2b9d978dc270849c9b329f4ecd693578f07e08715`).
Both ELFs have16KiB alignment, RELRO/NOW and non-executable stacks. These public
synthetic tests access no app wallet, Keystore, endpoint or node. Evidence and
source/archive/artifact hashes are in
`.cache/android-wallet/signature-core-20260914/`.

Focused checks use `ctest --test-dir native/build/safety-active -R
'^wallet_signature' --output-on-failure`. `ZCL_ORACLE=ON` adds independent
OpenSSL verification, and `ZCL_FUZZ=ON` builds `fuzz_signature`.

## Verified canonical P2PKH input scripts — 2026-09-14

Internal `zcl_signature_p2pkh` accepts only public signature/digest/key-hash
data. It requires strict DER8..71, low-S, a compressed33-byte public key,
exact32-byte digest and20-byte P2PKH hash. After copying all inputs, it parses
the key, compares SHA256/RIPEMD160 with the supplied destination, parses and
re-encodes the signature to establish exact canonical DER, and verifies the
supplied digest. It then emits two minimal direct pushes: DER plus the one-byte
SIGHASH_ALL selector, followed by the compressed key. Total length is DER+36,
bounded44..107. It never adds a transaction vector length or terminator.

Every provider status and returned length is checked before use. Only the
successful prefix and length publish; both outputs remain untouched on every
failure. Unused signature bytes/padding are ignored. The complete private work
clears after every entered path, with no new heap, RNG, secret or retained
pointer. The caller must keep all inputs stable and nonoverlapping during the
synchronous call. The helper has no network/chain/ownership/consent semantics;
its future wallet caller must obtain the digest and P2PKH hash from the same
exact owned review and recheck its own operation/completion lifetime.

The original Zclassic `script/sign.cpp` and `script/script.h` at
`14a83d510ffd109d3fa09bf74ebf8c28854a263f` were inspected directly from Git
objects. They establish signature-hash-byte appending, P2PKH signature/key stack
order, and direct pushes below OP_PUSHDATA1. Exact source SHA256 values are
`bd433d4a7886384af1f1a4a11dd2c49a1cd2ea7a7d251cc59048301b28e44351`
and `f91b4462713a3df7d1bdd665c71bf3c86a743039baa9d2a2f1b8182dae460af0`.
No original node/script interpreter is executed, and this narrow wallet profile
does not claim general script or consensus acceptance.

Tests cover64 signing profiles across129 capacities plus SIZE_MAX capacity,
known generator HASH160, ignored DER tails, every used signature/key/digest/hash
byte mutation, all lengths0..73 and SIZE_MAX, actual high-S, nonminimal DER,
trailing bytes, zero scalars, NULLs and both output guards. The two direct pushes
are independently read and checked for exact consumption. OpenSSL separately
checks the public key hash, canonical DER/low-S and exact-digest verification.
Fault providers mutate the original signature/length/key/digest/hash after the
first private copy and return dirty failure data, mismatching hashes, malformed
lengths or changed canonical bytes. Captured work spans are inspected and
retired only inside the live zero callback. Both returned outputs remain atomic.

All24 deliberate mutants are detected:23 by intended fixture assertions, and
the removed canonical-length check by ASan intercepting a SIZE_MAX memcmp.
All81 native ASan/UBSan/LSan groups pass in58.04 seconds. Clang/GCC production,
fixture/fault/oracle/fuzzer analysis passes with unchanged10/15 complexity caps
(495/1095 functions). The optimized host entry frame measures600 bytes.
GCC caught a test-provider array declaration mismatch, fixed to match the
RIPEMD160 header. The fuzzer's result checks were split to meet the existing
complexity cap, and local NULL/DER bounds now precede oracle-dependent arithmetic.
No warning, assertion or acceptance cap was disabled; initial/final logs remain.

Android/JVM/lint, APK alignment/fixture isolation and architecture gates pass.
APKs remain byte-identical because the helper has no JNI caller. Standalone
tests linked against the actual new release archives pass on x86-64
API30/35/36 (SHA256
`e5f518f3120cbe9e96d24676d708c454ab6d35df656a35634d3581cc8cc34477`).
ARM64 is compiled only (SHA256
`584b50d27accf28f594386fc2aac160ce7c4c413c3a9e1d59f3aa4a80308c006`).
Both ELFs have16KiB alignment, RELRO/NOW and non-executable stacks. These public
synthetic fixtures access no app wallet, Keystore, endpoint or node.

The final OpenSSL differential fuzzer completes1,792,794 executions in121
seconds without a finding, with max_len160, timeout5 and RSS512MiB (observed
peak274MiB). It compares arbitrary signature/key/digest/hash tuples, malformed
lengths, NULLs, capacities, both-output preservation and exact script contents.
The initial698,807-execution campaign remains separate from the final refactored
and locally bounded harness; original/final binaries, source and logs are retained.

Focused checks use `ctest --test-dir native/build/safety-active -R
'^wallet_signature_script' --output-on-failure`. Both `ZCL_ORACLE=ON` and
`ZCL_FUZZ=ON` are required for `fuzz_signature_script`, which compares arbitrary
public signatures with the independent OpenSSL oracle. Evidence and exact
source/archive/artifact hashes are retained in
`.cache/android-wallet/signature-script-20260914/`.

## Complete public signed wire from an owned review — 2026-09-14

`zcl_review_p2pkh_wire` copies the supplied candidate context and all1..8 public
signatures before provider work. It parses only the live owner's unsigned
transaction, requires every original input script to be empty, derives each
digest from that exact review and candidate, and verifies each canonical P2PKH
signature against its assessed input hash. Only the input scripts change.
The ordinary bounded transaction checker and serializer produce at most1925
bytes into private storage. Length equality and live-ID checks before and after
serialization precede publication of both bytes and length. Every failure
preserves both caller outputs. All entered work and per-row digests clear.

This internal public-data assembler has no secret, RNG, heap, JNI, signing or
consent capability. It does not consume or extend the review. The existing
exclusive adapter lock and stable, nonoverlapping input/output spans are required.
Its supplied `now_ms` is invocation time: repeated checks do not obtain a fresh
completion clock. A future authorized operation must authenticate current chain
context and consent, then recheck actual completion/delivery time. A successful
public assembly is never a cached authorization token or unspentness proof.

Real-provider tests cover both networks, every input count1..8 and1/16 outputs,
all capacities for one profile, exact boundary/SIZE_MAX capacities for32
profiles, every selected row's altered/swapped/wrong-digest signature, malformed
lengths, P2SH refusal, NULLs, stale IDs, rollback, expiry and replacement reviews.
Destroying the original source buffers leaves the owned review authoritative.
Independent reading verifies exact two-push consumption and every preserved
transaction field. The host oracle hashes the completed signed wire with the
qualified independent reader/libsodium and verifies its public signatures with
OpenSSL. The bounded differential fuzzer manually splices expected wire without
using the wallet parser, serializer or signer, and models complete owner state
and output preservation. Its synthetic fixture reference is deliberately limited
to its declared final-sequence/zero-lock profiles; it is not general consensus.

The20-mode failure suite covers dirty parser/hash/script/check/serializer output,
SIZE_MAX counts and lengths, copied-source mutation, short capacity, cancellation
before/during encoding, replacement ownership, rollback and expiry before final
publication. Live zero callbacks inspect and retire captures; cleanup counters
fail before any post-lifetime pointer-marker check. All26 deliberate mutants are
detected:25 intended assertions and one ASan negative-size copy interception.
Initial fixture complexity findings were fixed by named operations and an exact
stage table, retaining every assertion and the existing caps.

All83 ASan/UBSan/LSan groups pass in61.24 seconds; focused real/fault tests and
the independent oracle pass. Clang/GCC production and new fixture analysis pass
with unchanged complexity caps10/15 (500/1145 functions). Optimized host frames
are3304 bytes for assembly and2040 for private publication, each below4096;
these are individual frames, not the nested call-chain total. The final fuzzer
completes74,095 cases in121 seconds without a finding (max_len144, timeout5,
RSS limit512MiB).

Android/JVM/lint, APK alignment/fixture isolation and architecture gates pass.
Debug and test APKs remain byte-identical. The release APK changes only because
both native libraries' GNU build ID notes change; removing just those notes from
comparison copies yields exact byte equality. The unsigned release SHA256 is
`1126be153726695456f32138d75da8b7b7328fab3b4b958ec7718cb133cbb402`.
No new JNI caller exists. Standalone tests against
the actual new release archives pass on x86-64 API30/35/36, executable SHA256
`1e2976628ef3c505fc3cc19d7fb72392b60dc7bb5909ec00153a7954f91c9a4c`.
ARM64 is compiled only, SHA256
`befe9e7793ddbeb964d8ea388515428ed518e64a951bf2cdad1a9ed90bcfa42a`.
Both ELFs have16KiB alignment, RELRO/NOW and non-executable stacks. These public
synthetic fixtures access no app wallet, Keystore, endpoint or node. This adds
no fresh camera or positive hardware-custody claim. Fresh debug API35
storage/record JNI tests pass (three tests, 13.859 seconds); the exact new
locally signed minified APK starts successfully on API30/36. Debug instrumentation
cannot run against release's removed Kotlin classes; the initial mismatched
attempts are retained as failures, not counted as release JNI acceptance.
Exact evidence and hashes
are in `.cache/android-wallet/review-signed-wire-20260914/`.

## Ordered continuation

1. Authenticated key/change ownership and durable index recovery, using the
   existing exact draft construction/assessment/review binding. A server balance
   or history assertion cannot provide spending authority. The consumed-change
   address operation in [CHANGE_STORAGE.md](CHANGE_STORAGE.md) now verifies
   recovered wallet identity and the observed journal head without reserving
   again. The internal live-review comparison above now matches these keys to
   exact reviewed inputs. Platform-authenticated composition with current-chain
   context and consent remains open; no checked result is an approval token.
2. Bind current branch/height and authenticated authorization to the exact
   live review. The internal P2PKH digest now obtains scriptCode, input amount
   and transaction bytes only from that review; it grants no signing authority.
   The candidate-context wrapper above now selects the pinned branch and checks
   owned expiry/finality against explicit height/time inputs; it does not
   authenticate their source or establish current chain state.
   The [repaired BLAKE2b public-data helper](BLAKE2_REVIEW.md) now passes strict
   analysis, independent vectors, fault/sanitizer/fuzz and standalone Android
   checks. The narrow transparent v4 construction also has independent public
   hash evidence above; authenticated authorization remains open.
3. Synthetic signing and explicit review/cancellation, then qualified broadcast
   lifecycle and restart recovery. The bounded raw-digest primitive above now
   has independent host verification and real release-library Android evidence;
   canonical P2PKH input scripts now also have strict public verification.
   Complete public signed wire now has exact-review and independent host/device
   evidence above. Authorized live-review completion remains open.
   Real funds remain outside development tests.
4. Shielded wire/proof/witness/value/recovery qualification before exposing it.

TLS remains **BLOCKED — REQUIRES FURTHER SECURITY REVIEW** and excluded from
normal builds. Positive hardware custody still requires a qualified device.
