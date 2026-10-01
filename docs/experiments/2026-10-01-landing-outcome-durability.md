<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Landing terminal-outcome durability

On 2026-10-01, the Hetzner development checkout exercised the publication
recovery boundary in `dev.land` on Linux x86-64. The landing queue already
flushed its replacement file and parent directory before remote mutation, and
the publication CAS already flushed staged object bytes plus its temporary,
shard, and object-store directories. Terminal `outcomes.jsonl` records did not
have the equivalent barrier: they were appended and closed before the live
queue row could be removed.

The landing adapter now encodes the complete bounded terminal history into a
staging file while holding the queue lock, flushes it, atomically replaces the
regular outcome file, and flushes the parent directory before treating a
terminal record as the recovery checkpoint. A failure before rename leaves
the previous complete history in place, removes the staging file, and keeps
the live row. An ambiguous parent sync after the complete replacement was
renamed also keeps the live row. A replacement worker recognizes the matching
outcome, flushes that existing record, and then removes the queue row; it does
not publish a duplicate or contact the publication target again. Ordinary
diagnostic and mail appends remain outside this checkpoint path.

One registered regression injects a staging refusal after the complete new
history is written but before its flush and proves that neither
`outcomes.jsonl` nor its staging file is published, while the remotely moved
row remains reclaimable. Another injects the failure after the complete
replacement is renamed but before its parent sync. It observes the real remote
fast-forward, the retained queue row, and one complete outcome record. It then
makes the disposable bare remote unavailable. The next landing step completes
from the local terminal record, leaves its bytes unchanged, removes the queue
row, and reports an empty queue on the following step. All repositories and
state used by the tests are fixtures under the checkout's ignored `test-tmp`;
no operator datadir or wallet is opened.

The exact test-fast runner
`2d8196a94fa0dea37f541e16318eac46e75c519c65bf6a3a56065017ce7577bb`
ran `test_dev_land` cold: 1 registered group ran, 0 failed, 0 skipped, and
1,232 were gated by the exact selector in 130.8 seconds. `lint-fast` passed
all 33 gates. Test registration reported 1,068 dispatched entry points and no
canonical-registry drift; the no-shellout, C23-only, unattended-publication,
and silent-error ratchets passed. The generated capability inventory records
1,517 capabilities, 21,149 symbols, 1,804 duplicate candidates, and 800
untested invariants.

`lint-preflight` passed API-reference generation, capability-inventory
generation, POSIX-ERE, and no-wall-clock gates. Its capability-closure gate
remained red on four unrelated, unchanged rows in
`package_swarm_resume.c`, `build_action.c`, and `package_swarm_node.c`; the
gate's own 21-case self-test passed. This result is not claimed as a green
preflight and those files were not absorbed into this publication-recovery
slice. A MinGW compiler was not installed, so the Windows cross-syntax route
was unobserved; the registered Windows platform-seam and acceptance-guard
gates passed in `lint-fast`.

The retained development verifier was restored after build-driven relinks and
verified at
`bbe1428b9ab2cbc654ef7070edace75ef2771cfe5e57675f5aef84f14f8d6c72`.

## Submission queue admission

The same audit found that a newly appended `queue.jsonl` row could be
acknowledged after `fclose` without an explicit file-and-parent durability
barrier, and a process death during that append could leave a torn final row.
Submission now encodes the complete bounded queue into a staging file,
flushes it, atomically renames it, and flushes the parent directory while
still holding the row lock. A failure before rename leaves the prior complete
queue and removes the staging file. If the post-rename directory sync is
ambiguous, the command reports the retryable `QUEUE_SYNC_FAILED` state and
keeps the complete new queue. An identical retry finds that row under the same
lock, flushes it, and returns its original sequence as a deduplicated request;
it neither appends another row nor schedules duplicate proof work.

One registered regression injects a staging-file flush failure and proves
that neither `queue.jsonl` nor its temporary file is published before an exact
retry creates sequence 1. A second injects the failure only after the complete
queue is renamed. It observes the blocked, retryable, mutated reply, preserves
the queue bytes, retries the exact immutable tip and checkout, and observes
one durable row with sequence 1. The exact test-fast runner
`69a09430daa6001af1e9a1702c7bb21dc592ffaa03db986bb80561609f45cebc`
ran `test_dev_land` cold: 1 registered group ran, 0 failed, 0 skipped, and
1,232 were gated by the selector in 135.3 seconds. The initial regression run
was red before reaching the injected barrier because its unsigned fixture had
omitted the required proof stub; adding the same test-only proof precondition
used by the neighboring valid-submit fixture made the intended path
observable without relaxing production admission.

## Landing state directory authority

The landing leaf previously created `land/` and `land/logs/` with raw
`mkdir`, then accepted any existing path for which `stat` reported a
directory. Because `stat` follows links, an existing symlink could redirect
queue, lock, log, intent and outcome state outside the owner-private state
root. Both directories now use the platform owner-private directory
abstraction, which validates ownership and mode and refuses POSIX symlinks or
Windows reparse points. The generic directory helper used for disposable
dependency materialization was not changed.

The registered POSIX regression creates an isolated state root, points its
`land` entry at a separate directory, and invokes the real status action. The
leaf returns `STATE_DIR_FAILED`, and the test verifies that no `logs` entry was
created through the link. This is a state-authority hardening only; it does
not change proof admission, Git ancestry, or publication policy.
