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
on Hetzner commit `7bf6e482a`; do not duplicate that change in this slice.
