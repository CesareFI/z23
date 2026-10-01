<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Landing terminal-outcome durability

On 2026-10-01, the Hetzner development checkout exercised the publication
recovery boundary in `dev.land` on Linux x86-64. The landing queue already
flushed its replacement file and parent directory before remote mutation, and
the publication CAS already flushed staged object bytes plus its temporary,
shard, and object-store directories. Terminal `outcomes.jsonl` records did not
have the equivalent barrier: they were appended and closed before the live
queue row could be removed.

The landing adapter now flushes the exact regular outcome file and its parent
directory before treating a terminal record as the recovery checkpoint. An
ambiguous sync after a complete append keeps the live row. A replacement
worker recognizes the matching outcome, flushes that existing record, and
then removes the queue row; it does not append a duplicate or contact the
publication target again. Ordinary diagnostic and mail appends remain outside
this checkpoint path.

The registered regression injected the sync failure after the full terminal
append. It observed the real remote fast-forward, the retained queue row, and
one outcome record. It then made the disposable bare remote unavailable. The
next landing step completed from the local terminal record, left its bytes
unchanged, removed the queue row, and reported an empty queue on the following
step. All repositories and state used by the test were fixtures under the
checkout's ignored `test-tmp`; no operator datadir or wallet was opened.

The exact test-fast runner
`2d3412068a758670202ee166a46385f1efe2441f5594eac887b7e60f1500bf27`
ran `test_dev_land` cold: 1 registered group ran, 0 failed, 0 skipped, and
1,232 were gated by the exact selector in 131.5 seconds. `lint-fast` passed
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
