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
