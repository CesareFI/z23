<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Node engineering coordination

Keep this file limited to durable cross-server findings. Exact remote branch
heads remain the integration authority.

## Current ownership boundary

- Hetzner owns peer lifecycle, download scheduling, stall recovery and network
  throughput on `agent/hetzner-zclassic-node-20260918`.
- Worldstream storage work uses `agent/worldstream-storage-20260920` for
  database/startup/resource and presentation responsiveness changes that do
  not overlap Hetzner's network surface.

## 2026-09-20: comprehensive explorer statistics during IBD

The explorer statistics cache builder performs presentation-only aggregate
queries across the large block, transaction, UTXO, address and shielded
projection tables. It could start from the boot prewarmer or an HTTP request
while synchronization was active, adding read and cache pressure to the same
database being advanced.

Worldstream now admits this comprehensive rebuild only in `SYNC_IDLE` and
`SYNC_AT_TIP`. Existing cached output remains immediately serveable during
IBD; an uncached request retains the existing warming page, and a later
idle/tip request retries. The policy is fail-closed for unknown future sync
states. No reducer, peer, block-request, consensus, serialization, monetary,
PoW, activation or cryptographic-validation behavior changes.

Focused explorer tests and their ASan/UBSan variants pass all three registered
groups without skips; GCC's static analyzer, the complexity ratchet,
architecture tree, C23 node build, consensus-parity and sealed-core gates pass.
Clang is not installed on this host, so Clang-specific observation remains for
an independent lane. The root-user flag-registry selftest issue is already fixed
on Hetzner commit `7bf6e482a`; Worldstream independently reviewed, ported and
validated that exact commit rather than duplicating it.

## 2026-09-20: transaction-index rebuild finalization

The transaction-index bulk-load finalizer previously discarded every SQLite
result while rebuilding its two lookup indexes, restoring normal durability
settings and truncating the WAL. It therefore returned success even when an
index was not created, allowing the background builder to persist its
`tx_index_complete` marker for an incomplete read projection.

The finalizer now checks and logs each operation and fails closed. A focused
fault-injection regression makes the connection query-only, proves finalization
is rejected, restores write access, and proves a normal finalization and lookup
still succeed. This changes only the rebuildable transaction lookup projection;
block and transaction validation, consensus serialization and chain state are
unchanged.

The same rebuild path also disabled WAL autocheckpointing for the entire raw
block-file scan. It now shares node.db's existing bulk limits (approximately
256 MiB between checkpoints and a 256 MiB retained-file cap), then restores the
normal checkpoint, journal, cache and busy-timeout settings during finalization.
The focused lifecycle test reads the live PRAGMAs in both phases so a future
unbounded setting or incomplete restoration fails deterministically.

## 2026-09-20: Worldstream publication boundary

This checkout is a shared-checkout lane: its installed pre-push hook admits
only `refs/heads/main`, and the lane contract says agents commit locally but do
not push. Because Worldstream is prohibited from pushing main and no approved
development-ref publication command exists, the validated storage commits
remain on the local `agent/worldstream-storage-20260920` branch for an
orchestrator or owner-controlled integration. Do not bypass the hook.

## 2026-09-20: writable database connection tuning

The writable node.db open path previously discarded errors from its PRAGMA
batch and busy-timeout setup, allowing startup to continue with a partially
configured connection. The open path now checks the bounded batch, rejects and
closes an incompletely tuned handle, and reports failure to its caller. A
SQLite authorizer regression deterministically denies PRAGMA operations, proves
the tuning helper fails closed, then removes the fault and proves normal tuning
still succeeds. This changes connection setup only; schemas, stored bytes,
chain validation, consensus serialization, monetary policy, PoW and
cryptographic validation are unchanged.

The explorer's short-lived read-only connections now apply the same fail-closed
contract: failed mmap tuning or busy-timeout setup closes the handle before any
page or API query can use it. Fault injection denies the tuning PRAGMA and
proves both rejection and normal recovery.

Short-lived writable runtime reopens also used to discard their connection
PRAGMA and busy-timeout results. They now use a separately tested fail-closed
helper that preserves the established WAL mode and smaller 2 MiB cache while
rejecting and cleaning up any partially tuned handle.

## 2026-09-20: startup/restart measurement and boot-timing atomicity

An isolated height-zero node measured 17.233 s fresh, 17.248 s after a clean
shutdown and 16.969 s after an unclean termination to RPC-ready. SQLite open
and migration measured 161 ms fresh and 10--11 ms on both restart paths; the
dominant stage was proving-parameter initialization at 16.4--16.8 s. Peak RSS
was 909--992 MiB. The same fresh fixture under an isolated home with no proving
files used the compiled-in verifying keys, reached RPC in 546 ms, and peaked at
189 MiB. This identifies proving-key loading as the remaining startup/resource
risk, not SQLite recovery. It crosses the cryptographic-validation ownership
boundary and was deliberately not changed by this storage slice.

The boot flight recorder did expose an independent write-path defect: each
stage row and retention delete ran as a separate implicit transaction, so a
failure could durably preserve only part of one boot observation. All timing
rows and retention now share one checked transaction; any insert, prune or
commit failure rolls the sample back and suppresses the restart-loop check for
that unrecorded boot. A statement trace proves one begin/commit owns a normal
sample, and a planted middle-insert failure proves no earlier or later stage
survives. The focused syscall fixture reported 18 fsync calls before and after
because its setup and direct history seeding dominate that aggregate, so no
wall-clock or fsync-count speedup is claimed. Consensus impact: NONE. Hetzner's
network/block-swarm work is unaffected. Next storage investigation: bound the
proving-parameter loader's transient whole-file allocation only through the
repository's cryptographic-core review/unseal process, or continue with
LevelDB/index write efficiency without crossing that boundary.

## 2026-09-20: legacy transaction-index record hardening

The LevelDB block and transaction-index write entry points have no production
callers in the current node; live persistence is SQLite, while the legacy
LevelDB transaction index remains a read-only compatibility source. Optimizing
that dead write path would therefore not improve IBD throughput.

The live compatibility reader instead exposed a correctness risk: a found but
truncated varint record returned success with a zero or partially decoded disk
position. It also narrowed unchecked 64-bit values and accepted trailing bytes.
The reader now decodes into temporary state, rejects truncation, overflow,
out-of-range fields and trailing data, and publishes the position only after a
complete parse. The failure policy is fail closed with context; the caller's
output remains unchanged. Focused fixtures cover truncation, overflow, trailing
data and a valid legacy record. Consensus impact: NONE; this only hardens an
auxiliary lookup path and does not change block/transaction validity,
serialization, PoW, monetary policy, activation or cryptographic validation.
Hetzner's network/block-swarm work is unaffected. Remaining risk: native raw
structure records are retained for existing local compatibility and should be
separately inventoried before considering removal.
