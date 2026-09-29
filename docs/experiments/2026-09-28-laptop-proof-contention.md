<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Laptop landing-proof contention

On 2026-09-28, an x86_64 AMD Ryzen 7 PRO 8840U laptop with GCC 16.1.1 ran
`dev proof step` for signed local commit
`a0347c7c81e8df9784b54cc1878bf75d154843fb` against remote base
`45e9c4c3c7bcb6a6cad03ea4b1ba7a64415e2c8b`. The proof's cold generation
and `test-needs` passed. Its advisory preflight selected 1,219 required groups
with no admitted reuse. The test runner began its 16-worker pool after 10
exclusive groups; lint began after that exclusive pass.

The runner reported 300 seconds of no output for `test_crypto_registry`,
`test_dev_platform_shard_02`, `test_dev_platform_shard_03`,
`test_devagent_ticketkey`, and `test_character_sheet`. The captured group logs
had reached their ECDSA benchmark, private-worktree source identity checks,
checkout-key query, and character-sheet checks, respectively. Full lint also
failed its unchanged 600-second outer wall budget in `build-epoch-selftest`
while running beside the test pool. The worker was stopped after these
decisive failures, at 4,734,408 ms elapsed; no successful proof receipt or
suite verdict was claimed. The failed attempt's phase and child logs remain
under `.cache/zcl-dev-proof/attempts/` in this checkout.

The strict `build/bin/test_parallel` runner was then built from the same source
plus the proof-scheduling seam under edit. Each reported group passed alone
with zero skips under the same 300-second silence watchdog:

| Group | Isolated wall time |
|---|---:|
| `test_crypto_registry` | 11.8 s |
| `test_dev_platform_shard_02` | 18.5 s |
| `test_dev_platform_shard_03` | 11.3 s |
| `test_devagent_ticketkey` | 22.2 s |
| `test_character_sheet` | 18.7 s |

Each group's log was kept on the laptop. These solo passes show
that the recorded watchdogs depended on concurrent load; they do not establish
that the next full proof passes.

The proposed scheduler waits for the whole bounded test child before starting
lint, retaining both verdicts and all budgets; the integrated scheduler applies
that order on hosts with fewer than 24 available CPUs. The runner's exclusive
pre-pass now includes these five groups. The selector policy group passed,
and a strict `test_crypto_registry` selection reported
`exclusive pass done groups=1` and passed in 11.8 seconds. Full proof remains
the acceptance gate for the combined schedule.

## Full strict suite on the first scheduling repair

The signed scheduling repair `52407e8b0` ran
`make -j16 test-parallel TEST_PARALLEL_ARGS=--no-cache` without a foreground
lint command. The runner completed 15 exclusive groups. It reported 1,212 groups
run, 11 gated, 19 self-skips, three load-flaky recoveries, and one failure in
1,607.1 seconds. None of the five prior watchdog groups failed. The sole
failure was `test_codeindex_scale`: its 500,000-file build went silent for
301 seconds under the shared pool. Its isolated retry completed the build
but failed the unchanged 30x linearity assertion: 125.2 seconds for 500,000
files versus 3.0 seconds for 50,000 files, a 41.2x ratio. The suite log was
kept on the laptop.

Two instrumented repeats were initially believed to be isolated. The first
refused Merkle source enumeration for the 500,000-file fixture, before the
timed build completed. The second passed the unchanged linearity assertion: 14.0
seconds for 50,000 files and 37.9 seconds for 500,000 files, a 2.7x ratio.
For that second run, 50,000-file scan/write took 5.9 seconds and its root
recheck 1.7 seconds; 500,000-file scan/write took 13.2 seconds and its root
recheck 4.9 seconds. Temporary phase instrumentation was removed after the
measurement. Both logs were kept on the laptop.

Process inspection later found a surviving `z23-dev dev proof ensure` from
the earlier proof, adopted by the user service manager in this worktree. It
started `make ff` during the full-suite and instrumented runs. Those runs
therefore measured a competing producer, even though no lint was started by
the foreground suite command. The exact worktree-local process was stopped;
its descendants exited. The instrumented ratios above are retained as
observations under contention, not idle-host performance evidence.

With that producer gone, strict `test_codeindex_scale` ran in the exclusive
pre-pass and passed: 3.148 seconds for 50,000 files and 36.817 seconds for
500,000 files, an 11.7x ratio under the unchanged 30x limit. The group
reported one exclusive group, zero failures, and zero skips in 67.8 seconds.
The 300-second silence watchdog also remains unchanged.

## Strict suite on signed source

Signed commit `043411d4b` ran the canonical uncached strict suite with no
background proof worker or concurrent lint command. The optimized C23 node
check passed, followed by all 16 exclusive groups. The suite then reported
1,223 total groups, 1,212 run, zero cached, 11 gated, zero failed, 19
self-skips, zero unobserved environment results, and 1,753.9 seconds of test
body wall time with 16 workers. Three groups failed their first shared-pool
attempt and passed the runner's single idle-pool retry:
`test_dev_platform_shard_01`, `test_dev_fleet_start`, and `test_supervisor`.
They remain reported as load-flaky, not silent passes.

## Full lint compile-probe contention

The first separate `make -j12 lint` run executed 213 gates in an eight-worker
lint pool. It passed 212 and refused `check-build-epoch-integrity` after
688.7 seconds. Its `build-epoch-selftest.sh` reached the unchanged 600-second
outer wall limit at `phase=late-marker-refusal` with load averages 6.08,
4.90, and 4.85. The companion depfile-scope self-test passed.

With the lint pool idle, `make check-build-epoch-integrity` passed uncached:
the toolchain-keyed epoch, source-bound publication, concurrent publication,
late-marker refusal, make recovery, degraded-probe refusal, and depfile-scope
checks all retained their assertions.

The lint driver now schedules `check-build-epoch-integrity` in its serial
prologue before compiler sweeps. A second `make -j12 lint`, using a fresh
temporary build-epoch probe cache, passed all 213 gates in 534.3 seconds.
The uncached build-epoch gate passed in 148.6 seconds. Its 600-second outer
limit and every assertion remain unchanged. This validates the local lint
schedule.

## Exact proof checkpoint and upstream integration

At 2026-09-28T21:49:20Z, an isolated unarmed checkout completed
`dev proof step` for signed commit
`a2d3adec08fea2aac7f0679a58b53d9c9c477667` against exact base
`45e9c4c3c7bcb6a6cad03ea4b1ba7a64415e2c8b`. The cold receipt reports
`status=passed`, `receipt_reused=false`, and 4,451,540,989 microseconds
elapsed. The strict test log reports 1,219 groups run of 1,223, four gated,
zero failed, zero self-skipped, zero environment-unobserved, and four
load-flaky idle retries. Full lint passed all 213 gates. The exact receipt
is `.cache/zcl-dev-proof/receipts/a2d3adec08fea2aac7f0679a58b53d9c9c477667-45e9c4c3c7bcb6a6cad03ea4b1ba7a64415e2c8b.receipt`
in that isolated checkout. The receipt validates the stated pair only.

After fetching the newer `origin/main` at `a04a93ff6`, the merge conflicts
were the generated capability inventory and a declaration adjacent to the
Linux clang runtime check. The inventory was regenerated from merged C23
source: 1,513 capabilities, 21,003 symbols, 1,770 duplicate candidates,
and 790 untested invariants. The merge also needed the existing portable
file-shape API in `devloop_facts_consumer.c` for a newly added Makefile
reader. The merged `make -j12 z23` passed the C23 node and glibc ABI check.
Registered focused selections passed: build fabric 2/2, semantic 10/10,
platform 9/9, ROM fetch 4/4, and devagent 22/22 groups, all with zero group
failures. The semantic run had one self-skip in `test_semantic_facts_fuzz`:
its host-runpath lookup found no ELF clang beside the libclang linked by the
sensor. These focused checks do not certify the merged commit; its exact
proof and full lint remain required before push.

The first exact proof of merge commit `8930ee658` refused after 63.9 seconds
at the generated-document gate: `docs/CAPABILITY_INVENTORY.jsonl` still
identified the source before the final portable Makefile reader edit. No
test or lint verdict was claimed. Regenerating from the committed source
changed only the inventory metadata record; the census stayed at 1,513
capabilities and 21,003 symbols. The generator self-test and exact generated
check passed, with 1,177 registered roots resolved. A new signed commit is
required because an exact proof receipt cannot transfer to changed bytes.

Before retrying, `origin/main` advanced from `a04a93ff6` to `dc286ed32`.
The 11 reviewed commits repair test scheduling and resource assumptions,
preserve bounded load-flaky failure excerpts, and refuse incompatible
multi-host binaries before transfer. The only merge conflict was the
generated inventory. It was regenerated from the combined source; the
generator self-test passed, with the same 1,513 capabilities and 1,177
registered roots resolved.

`origin/main` then advanced to `ac6881ff9` with a Darwin live-descriptor
count repair and its generated catalog update. The merged catalog was
regenerated and passed the same exact check and self-test. The registered
`test_os_proc` group passed 1/1 with zero skips on the merged source. The
next exact proof at that point used `ac6881ff9` as its remote base.

## Same-size CAS overwrite missed by a metadata recheck

The cold exact proof for signed commit `caf07b804` against `ac6881ff9`
passed its 1,382,996 ms prefork build and selected 1,222 groups with no
test reuse. Its suite ran all 1,222 selected groups, with four gated, nine
load-flaky groups that passed alone, one self-skip, and one hard failure:
`test_metaverse_catalog` failed both in the shared pool and alone at the
mutation-between-hash-and-final-recheck assertion. Its suite log is in that
proof attempt's directory under `.cache/zcl-dev-proof/attempts/` in the
isolated checkout. No passing proof receipt is claimed for this pair.

On the x86_64 AMD Ryzen 7 PRO 8840U host, a separate C23 probe compiled
with GCC 16.1.1, `-std=c23 -O2 -Wall -Wextra -Werror -pedantic`, performed
10,000 same-size overwrites on a private `/dev/shm` file. Its `fstat` size,
mtime, and ctime snapshots were unchanged after 9,991 writes. The probe's
exact output was `same_snapshot_after_overwrite=9991/10000`. This establishes that a
metadata-only final pass cannot detect every in-place chunk mutation on
this host. The verifier now re-reads and rehashes every chunk under a
physical byte budget of twice the maximum package size (128 MiB), while
retaining the existing operation cap. The repaired `test_metaverse_catalog`
selection passed 1/1 with zero skips and explicitly checked both the
mutation refusal and the two full content passes. Removing absolute home
paths from this experiment also made `make check-no-operator-paths` pass its
planted-violation self-test and tracked-tree scan. A new exact proof remains
required for the changed bytes.

The full lint pass recorded 213 passing gates and one complexity-baseline
failure: the verifier function shrank from M=30 to M=27. Regenerating the
shrink-only baseline with `build/bin/z23-lint check-cyclomatic-complexity
--write-baseline` and rerunning `make check-cyclomatic-complexity` passed its
self-test and all 4,104 pins. The full lint result predates that regenerated
baseline; the integrated candidate still needs its final lint pass.

Reviewed main through `bce2d07a4` adds durable release replay, incomplete
carrier refusal, a historical Arena source fixture, and a proof-lane inventory
dependency. The merged capability inventory was regenerated and passed its
exact-output check. The merged complexity baseline passed its self-test and
all 4,104 pins. Focused cold test selections passed without skips:
`metaverse_catalog` 1/1, `zcode_store` 1/1, `zcode_swarm_net` 5/5, and
`dev_lane` 1/1. Full lint and exact proof were the next gates at that point.

## Integrated exact-proof checkpoint

Signed `9c22d2eec608d9bdb442131c58d25c389d68d4b6` against reviewed
`bce2d07a4b81c4eafe6218213e1d77063b4043f8` passed local full lint.
Its cold proof built the prefork bundle in 1,521,886 ms, then ran 1,222
registered groups with zero reused results, zero hard failures, seven
shared-load failures that passed idle retry, and one self-skip in
`test_semantic_facts_fuzz` because the host lacks an ELF clang beside the
linked libclang prefix. Four groups were host-gated. The proof did not pass:
independent lint passed 213 gates and failed `check-ship-remote-transaction`.
The slow remote-leg fixture returned exit 3 as expected but reported
`UNVERIFIED` after one observation instead of the expected `SLOW` label under
parallel load. The same gate passed when run alone on the unchanged branch;
the local full lint run had also passed. No assertion or refusal was weakened.
The failed proof attempt remains under `.cache/zcl-dev-proof/attempts/` in
the isolated checkout. It ran for
4,912,160,775 microseconds and returned `child_proof_failed_exit_2`.
There is no passing exact receipt for this commit/base pair.

## Later upstream integration and local gates

The merge of `origin/main` at `63cacd894dc663bfef2b51e23c27dd4c0c7afed9`
retained the physical three-host result. Its verifier contract suite passed
6/6 focused groups; `test_devloop_early` and `test_semantic_build_inputs`
each passed. Full lint reported 211/214 passes under eight concurrent gate
jobs. `check-doc-counts` found this log's historical denominator after the
registered catalog grew; the phrase was corrected and the gate passed alone.
`check-windows-cross-syntax` found an unguarded `lstat` in the make-value
glob reader; Windows now leaves that result unvouched, and the standalone
gate passed 2,387/2,387 translation units. `check-ship-remote-transaction`
failed in the concurrent run and passed alone with its rollback and
process-qualification fixtures. These are local gate results, not an exact
commit receipt or remote acceptance.

The subsequent merge of reviewed `8ea1782f063d48d20823b782639da0e493eacd90`
regenerated the capability inventory from the combined tree. Its local
`make z23-dev` build passed. Focused cold registered groups passed:
`proof_ticket_reuse` 1/1, `build_fabric` 2/2, `dev_land` 1/1, and
`devloop_early` 1/1. Full local `make lint` passed all 214 gates. This
qualifies the local merged source; the exact commit/base proof receipt and
remote push remain separate gates.

Reviewed `e6b20f4b2d9ebce160f3bc146430306eb622ccc0` adds exact reviewed
base/head pins to the landing attach path, a bounded proof wait, and refusal
of alternate include spellings in early reuse. After integration, the local
dev build and cold `devloop_early` and `dev_land` groups passed. Generated
API and capability catalogs matched their generators; Windows cross syntax
compiled 2,388/2,388 translation units; command-contract and lint-gate
wiring checks passed. These gates establish the merged local checkpoint,
not a passed exact push receipt.

## Runtime library bound on laptop A

The Linux fixed-compile attach fixture refused its runtime closure on this
host when the verifier resolved 144 shared libraries. Its parser previously
allowed at most 64 resolved paths. A laptop-local candidate raised that
parser limit to 256 while keeping the 16 KiB loader-output capture bound and
hashing each accepted path and its file SHA3-256 into the runtime root. With
that local change, on 2026-09-29, `make -j8 t-fast ONLY=build_fabric` passed
both selected groups, including `build_fabric_attach`, cold with zero skips,
and `make -j8 lint` passed apart from the regenerated capability inventory.

That raise loosens a fail-closed bound, so it is held for review and is not
part of the source this record accompanies: the parser still refuses more
than 64 resolved paths. The 144-library closure observed here remains an open
host-compatibility question, not evidence that the exact push proof passed.
