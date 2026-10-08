<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Current canary observation boundary

Base: `a604b833dbebc2e56bc23cb49ccb98e91d37a409`.
Before candidate preparation, current `origin/main`
`e0aa31975b9fcd30c11125ab50f3fadf6fafa931` was fetched and integrated by
fast-forward. Its changes do not overlap the four owned files; the reviewed
service and test bytes remain unchanged by integration.
The canary watcher retains verdicts after file removal or a corrupt replacement.
Consumers previously could not distinguish retained PASS display from a current
observation. The existing state dump now adds per-kind boolean `present`,
`parsed`, and `current_pass` without changing sentinel input or FAIL authority.

`present` means the filename was observed in the latest scan attempt; `parsed`
means that attempt read a JSON object. `current_pass` uses the existing exact
source, exact executable, PASS, and process-start clear predicate. Each attempt
resets only these observations before resolving its directory. An unavailable
directory and removed or corrupt files cannot retain positive current evidence.
Historical verdict and FAIL details remain available; removal never clears FAIL.
Filename kind remains authoritative and the sentinel's `from` stays display-only.
Concurrent dumps may observe a partial scan. File changes after a scan are learned
at the next scan; these flags do not claim continuous filesystem presence.

The existing clear predicate does not reject future `started_ts`, enforce
`ts >= started_ts`, or apply a maximum age. Therefore `current_pass` cannot
establish fresh genesis acceptance or the required retained 168-hour C8 window.
Reporter binding, explicit timestamp qualification, live coarse matching, and
retained exact source/artifact window evidence remain separate continuations.
No consensus, FAIL predicate, receipt acceptance, or production operation changes.

Registered acceptance: `canary_sentinel_watch`. New production-dump assertions
require boolean field existence and cover exact PASS, empty and inaccessible
scans, removal, atomic corrupt replacement, UNKNOWN, exact replacement, retained
FAIL, inclusive process-start boundary, stale/zero start, source/artifact mismatch
and invalid/missing identities, and filename genesis versus display-only anchor.
Source checks and behavioral execution evidence are recorded separately.

Designed mutations: remove the observation reset in
`canary_sentinel_watch_tick_once` while retaining dump fields; removal and
inaccessible-directory assertions must fail. Replace `slot->current_pass =
authoritative_clear` with `slot->current_pass = parsed && is_pass` in
`process_sentinel`; identity and run-boundary assertions must fail. Restore each
production mutation before accepting the fixed result. These are proposed
mutation witnesses; no mutation execution or runtime acceptance is claimed here.

Source-only checks on 2026-10-08: `git diff --check` passed. The existing
native complexity gate passed with cap 15 and all 3,929 baseline pins exact;
no complexity baseline was changed. Compilation, registered acceptance and
fixed/mutated/restored execution remain NOTRUN for this candidate.

The filename observation is recorded before any concatenated read-path refusal.
The Linux boundary fixture creates private nested directories with `mkdirat` and
`openat`, enumerates a 4070-byte directory path, and creates its sentinel through
the retained descriptor. Its full read locator exceeds the watcher's 4096-byte
buffer: the dump must still report present=true, parsed=false, current_pass=false.
The fixture does not change process cwd and removes its descriptor-relative tree.
This exact Linux pathname witness is not a macOS or Windows runtime claim.
Restoring observation assignment after the path-size refusal must fail it.
