<!-- Copyright 2026 Rhett Creighton; SPDX-License-Identifier: Apache-2.0 -->

# Proof-history receiver recovery and package-store boundary

Coordination ref: `day26-evidence`. This is isolated local fixture evidence,
not public-node, peer-replay, or production BUILD/CHECK acceptance. Heavy runs
used `devbuild --wait`.

## Recovery path

The receiver pages the existing package store by immutable manifest root, at
most 256 summaries per call. Its issuer-sequence, observation-root, and
input-key indexes are projections rebuilt from signed CAS evidence. It stages
the whole receiver, checks signed ticket and checkpoint ancestry, retains
eligible PASS/FAIL contradictions and prior verified branches, then swaps the
projection under a final store generation and disk-catalog check. A failed
page, blob read, replay, ancestry check, or final check publishes nothing and
reports zero published counts. After interruption, replay restarts at page
zero; an unsealed cursor is never trusted across a crash.

The package store now admits beyond the former 4096 lifetime limit. It
rebuilds a root/chunk catalog from authoritative manifests on open, uses a
sorted root index for `O(log N + page_size)` page selection, and caps parsed
manifest trees and wire copies at 256. The root/chunk catalog and CAS set
remain proportional to corpus size. No ledger of proof claims was added.
An atomically written generation file only detects simultaneous-handle
changes before quota, GC, page continuation, and publication decisions. Its
absence or damage is repaired with a fresh epoch on open; manifests and CAS
remain authoritative. Final publication validates manifests, pin markers,
and known CAS paths against the open handle.

## Fixture matrix

| Case | Observation |
| --- | --- |
| 4095, 4096, 4097 actual-store objects | Complete manifest plus CAS admission and receiver replay passed at every count. |
| 8193 actual-store objects | Full admission and replay passed; the 4097-object catalog reopened and rebuilt before extension. |
| Pagination and concurrent mutation | Root-cursor pages enumerated 4097 roots. Mutation after page one returned STALE on continuation and guarded publish. A second handle's stale page and attempted admission refused. |
| Staged/incomplete blob | Rebuild refused; store reopen, chunk completion, and full replay passed. |
| Unreadable blob or missing chunk | Rebuild refused with zero published counts; reopen, repair, and replay passed. |
| Missing referenced ticket | Signed checkpoint replay refused; restoring the ticket passed. |
| Deleted checkpoint/history or prior live head | Replay refused missing ancestry or verified history and retained the old receiver; restoring signed history passed. |
| Contradictory eligible PASS/FAIL | Both exact roots survived reconstruction; policy refused reuse. The boundary corpus places at least one conflicting ticket beyond the first 256-root page; revoking the PASS issuer removes its eligibility and still refuses reuse. |
| Signed ticket/checkpoint forks | Both branches remained equivocation evidence; recovery did not switch a verified branch. |
| Restart mid-rebuild; resumed rebuild | Late scan/replay failure left the old projection; a new handle and full replay from page zero passed after repair. |
| Corrupt local receiver index | Signed CAS replay reconstructed the index. |
| Deleted committed manifest under an open handle | Final disk validation returns INCOMPLETE and refuses publication; the prior receiver remains intact. |
| Missing committed-manifest directory on reopen | Open refuses before orphan GC and preserves referenced CAS bytes. An interrupted fresh layout with only empty directories can complete. |
| Nonempty staging remainder beside a committed manifest | Open refuses and preserves committed CAS; only a provably empty remainder is cleaned. |

The final recovery rows passed the focused store gate. A cold receiver cannot detect deletion of
an entire signed tail if neither its prior head nor another authenticated source
exists; a live receiver detects rollback against its retained head.

## Exact fixture roots

| Claim | SHA3-256 root |
| --- | --- |
| Eligible PASS ticket | `bb5062a0ef62d036906a482961821ae88435a63bce93e9ec8b43deaad2dc3b34` |
| Eligible FAIL ticket | `d9cf8833940eea8be2a63bacfe35e3236989627b8b6454424fb02921b5d247ee` |
| Signed head after late-replay repair | `0110eb4a2af5cf92e0aff23a73ab71f60d8d8c5744722b36a56a6f2e24e0e478` |

The tests rederive these from exact signed wire bytes and compare recovered
entries. They identify only local fixture evidence.

## Measured cost versus corpus

Linux `test_proof_ticket_reuse`, opt-in actual store, one process. Population
is excluded from rebuild timings. CPU is process time; block I/O is
`ru_inblock`/`ru_oublock` operations, not bytes. Peak RSS is a process high
water mark, not a per-row allocation delta. Host and disk contention affect
wall time. These rows are from the final focused gate with the atomic
cross-handle generation file and interrupted-layout recovery.

| Phase | Objects | Wall | CPU | Peak RSS | Block read/write ops |
| --- | ---: | ---: | ---: | ---: | ---: |
| Populate | 4095 | 104.948 s | 3.790 s | 39,148 KiB | 10,576 / 131,040 |
| Rebuild | 4095 | 110.747 ms | 110.715 ms | 39,148 KiB | 0 / 0 |
| Rebuild | 4096 | 100.889 ms | 100.882 ms | 39,148 KiB | 0 / 0 |
| Rebuild | 4097 | 99.302 ms | 99.303 ms | 40,172 KiB | 0 / 0 |
| Reopen catalog | 4097 | 66.783 ms | 66.787 ms | 40,172 KiB | 0 / 0 |
| Rebuild after reopen | 4097 | 101.006 ms | 100.992 ms | 40,172 KiB | 0 / 0 |
| Populate remaining 4096 | 8193 | 66.458 s | 7.095 s | 42,220 KiB | 6,464 / 131,072 |
| Rebuild | 8193 | 239.137 ms | 235.077 ms | 42,220 KiB | 0 / 0 |

Each population object is a four-byte unrelated blob. This measures catalog
and receiver scan scaling, not large artifact bytes. The original RED witness
observed `VCS_PACKAGE_STORE_ERR_LIMIT` for distinct object 4097; the current
fixture requires and passes complete 4097th-object admission.

A separate in-memory signed-sync probe retained 4095, 4096, and 4097 tickets:
the 4095-ticket initial delta took 3.912 s CPU; one-ticket extensions took
4.46 and 4.85 ms. The issuer-sequence index addresses the measured repeated
ticket lookup. CAS checkpoint replay at 129, 513, and 2049 objects took
0.364, 1.486, and 5.920 s CPU. That growth did not justify another checkpoint
index, so checkpoint ancestry remains on the signed-history path.

## Residual limits and authority

- The 256 cap covers parsed manifests and wire copies. One compact package
  record, its unique chunk references, and each present CAS hash remain in
  memory. Metadata memory is still `O(packages + referenced chunks)`; the
  largest measured corpus is 8193. The existing 64 MiB per-package cap and
  configured byte quota remain; there is no 4096-package lifetime cap.
- Final publication performs an `O(packages + CAS paths)` disk validation.
  It sits outside the bounded page loop and catches raw deletion or mutation.
- Quota, pins, and GC retain their existing authority. Stale simultaneous
  handles refuse direct package-catalog writes. A long-lived swarm handle
  explicitly refreshes its catalog under the process lock before admitting
  a verified manifest. That refresh validates every previously observed
  manifest and CAS object, replays current pin markers offside,
  and swaps only after full validation. It retains live class, access, and
  replica hints for unchanged roots; a failed refresh keeps the old catalog
  and refuses the download. No persisted policy authority was introduced.
- A mismatch between signature-valid issuer tickets and a signed checkpoint
  root remains equivocation. CAS alone cannot distinguish an omitted
  alternative fork ticket from a false signed root. Receiver policy is
  unchanged.
- No production BUILD/CHECK execution has yet been avoided. The separately
  owned pre-claim executor adapter remains pending. No laptop replay
  acknowledgment or peer route has arrived in this checkout.

Focused reproduction: `Z23_PROOF_STORE_BOUNDARY_RED=1
Z23_PROOF_STORE_LARGE_CORPUS=1 devbuild --wait make -j2 t-fast
ONLY=proof_ticket_reuse`. The 8193-object case is opt-in because it performs
more than 16,000 durable object writes. The variable name preserves the
earlier RED witness.
