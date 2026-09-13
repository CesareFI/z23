# Transparent transaction development

The C codec handles a bounded transparent-only subset of the existing Zclassic
v4/Sapling wire format. It is an offline library with no JNI, endpoint, key,
signing, broadcast or send-screen entry point. Successful decoding establishes
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

## Ordered continuation

1. Checked synthetic funding/prevout amounts, output classification, change
   ownership and review binding to exact transaction bytes. A server balance or
   history assertion cannot provide spending authority.
2. Independently qualify the exact original serialization and branch-specific
   signature-hash construction with offline public fixtures. No signing until
   scriptCode, input amount, branch/height, outputs and authorization are bound.
3. Synthetic signing and explicit review/cancellation, then qualified broadcast
   lifecycle and restart recovery. Real funds remain outside development tests.
4. Shielded wire/proof/witness/value/recovery qualification before exposing it.

TLS remains **BLOCKED — REQUIRES FURTHER SECURITY REVIEW** and excluded from
normal builds. Positive hardware custody still requires a qualified device.
