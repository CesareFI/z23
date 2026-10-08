<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# MVP parity reporter evidence boundary

Intention: prevent a zero mismatch counter from qualifying public-node C8
without the exact replay and continuous parity evidence required by
`docs/MVP.md`. The previous classifier reported C8 `met` with an active
oracle, zero mismatches, and no canary evidence.

The classifier now requires a present genesis PASS, freshness within seven
days, matching running source and artifact identities, a current coarse
match, and retained identity-bound zero-mismatch parity coverage for 168 hours.
Each missing obligation produces `unknown` with a named reason. An explicit
canary FAIL or observed mismatch remains `unmet` and takes precedence over
missing or positive qualification evidence.

The live reporter does not produce these positive qualification claims.
The existing canary watcher exposes source and artifact identities, verdicts,
and timestamps, but retains historical slots after sentinel removal.
Its scan count and quiet FAIL latch do not establish a currently present,
fresh genesis PASS. The parity service exposes counters and checked heights,
but does not retain an identity-bound continuous 168-hour observation window.
The new positive flags therefore remain false in the live evidence collector.
This repair closes a false positive; it does not complete C8 acceptance.

Continuation: expose current sentinel presence through the existing canary
watcher and qualify its genesis verdict, timestamp, source identity, and
artifact against the running executable. Retain the complete 168-hour parity
window through the existing evidence owner, binding the oracle, exact source
and executable, interval, successful checks, mismatches, gaps, and freshness.
Only independently qualified facts may populate positive reporter evidence.
Existing `tools/mvp_gate.sh` remains the operator gate; its fresh exact genesis
and live coarse checks do not substitute for missing duration evidence here.

The registered `syncdiag_rpc` regression exercises the original zero-counter,
absent-canary counterexample, omission of each positive obligation, unknown
mismatch count, and negative-evidence precedence. Its positive case supplies
synthetic evidence to the pure classifier and makes no live parity claim.

Validation at source preparation: `git diff --check` passed. Exact candidate
build, focused execution, mutation sensitivity, and runtime acceptance are
`NOTRUN` until separately recorded. No production node or consensus predicate
was changed.
