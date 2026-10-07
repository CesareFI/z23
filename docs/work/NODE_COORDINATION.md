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

## 2026-09-20: transaction-index state corruption refusal

The SQLite transaction-index projection treated a present but truncated cursor
or digest as if no fold had ever run. A reproduced one-byte state value passed
both readers, so a later batch could restart at height zero over existing rows
and publish a digest unrelated to those rows. Cursor decoding also shifted into
a signed integer, creating undefined behavior for a high-bit value.

Cursor and digest readers now reject malformed widths, cursor decoding uses
unsigned arithmetic with an `INT64_MAX` bound, and batch, keyed-read and dump
consumers stop or fail soft when the cursor cannot be trusted. Focused fixtures
prove the pre-fix failure and post-fix refusal for truncated cursor/digest and
an out-of-range cursor, while preserving the valid round trip. Failure policy:
fail closed for the background fold, return busy for the public lookup, and
report an explicit dump error; no partial state is accepted. Consensus impact:
NONE. This is a rebuildable auxiliary projection; block/transaction validity,
serialization, PoW, monetary policy, activation and cryptographic validation
are unchanged. Hetzner's network/block-swarm work is unaffected.

## 2026-09-20: Worldstream storage publication inventory

The coherent Worldstream storage series intended for the CesareFI development
branch is:

- `15a4faf7e` defer comprehensive explorer statistics during IBD;
- `5a8c7e4fa` make the flag-registry unreadable fixture root-safe;
- `0a21515aa` fail closed on transaction-index rebuild finalization;
- `82db7da4a` bound transaction-index rebuild WAL growth;
- `fddf46571`, `8b28647bc`, `9f64e9c4b` fail closed on primary, explorer and
  short-lived runtime SQLite connection tuning;
- `8a1c66725` make boot-timing sample persistence atomic;
- `dd3f65ad6` reject malformed legacy LevelDB transaction-index records;
- `f9c65b069` reject corrupt SQLite transaction-index cursor/digest state;
- `fcf59a057` fail closed on address-backfill errors;
- `4d9446747` log transaction-index orchestration failures;
- `c7d6aa74d` reject malformed snapshot height metadata;
- `a611745a7` bind snapshot database attach paths; and
- `c51aa03ee` recover snapshot transaction cleanup failures; and
- `c14b2f3f0` publish snapshot UTXOs and anchor atomically; and
- `094e78880` report full node-state blob lengths; and
- `ff66b1e8d` check node-state writer bounds and binds; and
- `71a23fb92` preserve node-state read and delete errors; and
- `1a155d843` resume incomplete snapshot authority imports; and
- `338419708` resume snapshot recovery without its source artifact.

Integration-only commits `a9171ad03`, `7768bff78`, `e4df36146` and
`a6ef6bb01` preserve current `origin/main` history and generated inventory.

The latest committed engineering tip intended for publication is `338419708`.
Each slice passed its focused regression, applicable sanitizer/static analysis,
complexity, architecture, generated-inventory, consensus-parity, sealed-core
and production-build gates as recorded above. Consensus impact for the entire
series: NONE.

## 2026-09-20: address backfill completion integrity

The one-shot explorer address backfill ignored SQLite failures from connection
tuning, schema setup, transaction boundaries, statement reset/bind/step,
cursor completion, finalization and completion-marker publication. A
deterministic trigger reproduced the consequential failure: the address upsert
returned `SQLITE_CONSTRAINT`, but the worker still returned success and wrote
`addresses_backfilled=1`. That false marker suppressed future retries while the
rebuildable address projection remained incomplete.

The worker now checks every SQLite boundary, bounds its row counter, owns its
statements and connection through one cleanup path, rolls back a failed active
batch, and publishes the completion marker in the final checked transaction
only after the cursor reaches `SQLITE_DONE` and both statements finalize.
Measured after the change, the same injected write fault returns failure and
leaves no marker; removing the fault lets the next run reconstruct the expected
balance/count row and atomically publish the one-byte marker. The focused
`boot_phase` group passes in both the normal and ASan/UBSan harnesses; the
complexity, architecture, consensus-parity and sealed-core gates pass without
raising a threshold.

The broader boot selection passed 32/32 groups (two explicit network-stress
self-skips), the production C23 node built, `lint-fast` and `lint-preflight`
passed, and the full lint umbrella passed 202/212 gates. Its ten residual reds
did not name this slice: pre-existing txindex silent returns, checkout
hook/hardlink/Tor priming, root-mode unreadable-file selftests, missing
Clang/libFuzzer standalone fuzz tools, and existing ship/Windows fixture
selftests. Those are not suppressed or relabeled green.

Consensus impact: NONE. The worker writes only the rebuildable explorer
`addresses` projection and its advisory completion marker; block and
transaction validity, serialization, chain history, PoW, monetary policy,
activation and cryptographic validation are unchanged. Hetzner's current work
through `f261d245d` is confined to block-swarm ownership, ready-peer sharing,
manifest bounds, monotonic stall timing and timeout-scan measurement, so there
is no component overlap. Remaining risk: batches committed
before a later failure remain visible but are idempotently overwritten on the
next marker-free retry. Recommended next investigation: audit the adjacent
boot-time explorer/index backfills for the same false-completion pattern,
starting with unchecked transaction boundaries in the offline import helpers.

## 2026-09-20: transaction-index failure observability

The strengthened full-lint silent-error gate found two orchestration guards in
the earlier transaction-index hardening that returned a failed schema or cursor
read without adding call-site context. The lower-level routines already failed
closed, but an operator could not distinguish batch initialization from a
public lookup failure in surrounding logs. The batch COMMIT path also relied on
a compound boolean and emitted no contextual line for a commit-specific I/O
failure.

The service now logs schema initialization, batch cursor, public lookup cursor
and COMMIT failures at their owning call sites while preserving the existing
stop/busy behavior. The dedicated silent-error gate moved from red to green,
the focused `txindex_projection` group passes, and the complexity ratchet stays
green without a baseline increase. No schema, transaction ordering, cursor
value or public classification changes. Consensus impact: NONE. Hetzner's
`f261d245d` tip remains confined to block-swarm networking and does not overlap.
Remaining risk: rollback errors are cleanup diagnostics and remain subordinate
to the already-logged primary failure. Recommended next investigation: the
offline snapshot importer has unchecked DETACH/rollback cleanup and permissive
text height parsing that should be fault-injected before any change.

## 2026-09-20: strict snapshot height metadata

The offline snapshot importer parsed its untrusted `_snapshot_meta.height`
TEXT with `strtoll` but did not inspect either `errno` or the end pointer. A
focused real-import fixture established the red baseline: metadata
`1000000junk` was accepted as height 1,000,000, migrated all 1,200 fixture
UTXOs, and published snapshot authority and stage cursors even though the
metadata was malformed.

Height loading now uses the shared full-string, overflow-checked int64 parser
and rejects missing, malformed, non-positive or out-of-range metadata before
any destination write. The regression proves rejection and unchanged output
arguments, restores the valid height, and then proves the normal import still
reconstructs the expected UTXO authority, commitment and cursors. Extracting
the read into a small helper reduced the importer's measured cyclomatic
complexity from its pinned M=55 to M=51; the ratchet was lowered accordingly,
not relaxed.

The focused normal and ASan/UBSan groups pass, as do `lint-fast`, architecture,
consensus-parity, sealed-core and the production C23 node build. Consensus
impact: NONE. This is fail-closed input validation before an existing snapshot
import and changes no chain history, serialized consensus data, block or
transaction validity, PoW, monetary policy, activation or cryptographic
validation semantics. Hetzner's current `f261d245d` tip remains confined to
block-swarm networking and measurement, with no overlap. Remaining risk: the
importer's post-attach transaction and cleanup paths still deserve
deterministic SQLite fault injection. Recommended next investigation: prove
rollback and detach behavior under a failed bulk copy or authority epilogue.

## 2026-09-20: bound snapshot attach path

The importer constructed `ATTACH DATABASE` by interpolating the snapshot path
into a fixed 640-byte SQL buffer. A real snapshot fixture whose valid filename
contained an apostrophe established the red baseline: integrity and metadata
reads succeeded, but `ATTACH` failed with a syntax error and the otherwise
valid snapshot could not be imported. The same construction also made SQL
syntax depend on path contents and could truncate a sufficiently long path.

The attach boundary now prepares a constant statement and binds the path as a
SQLite parameter with transient ownership. There is no fixed statement buffer
and filename bytes are never interpreted as SQL. The apostrophe-path fixture
now completes the full 1,200-UTXO import and all authority/commitment checks.
The focused normal and ASan/UBSan groups, `lint-fast`, architecture,
consensus-parity, sealed-core and the production C23 build pass. The refactor
also reduced the importer complexity pin from M=51 to M=50.

Consensus impact: NONE. Only local snapshot file selection changed; schemas,
stored consensus bytes, chain history, validation, serialization, PoW,
monetary policy, activation and cryptographic semantics are unchanged.
Hetzner's `f261d245d` work remains in block-swarm networking and does not
overlap. Remaining risk and recommended next investigation: fault-inject the
bulk-copy rollback path and verify the prior UTXO set, transaction state,
attached-schema state and progress handler are all restored before returning.

## 2026-09-20: snapshot failure cleanup recovery

The bulk-copy failure path discarded rollback and detach results, then logged
that node.db had been rolled back unconditionally. A deterministic authorizer
fault denied the UTXO copy and the first rollback. The red baseline returned
failure while leaving the write transaction open, `snapsrc` attached and the
prior UTXO hidden by the still-active delete; a retry on that connection was
therefore unsafe.

Rollback and detach are now checked bounded operations with two attempts and
per-attempt diagnostics. The progress handler is removed before detach, and a
failed commit follows the same cleanup path. BEGIN failure also uses checked
detach cleanup. The focused fixture denies the copy, first rollback and first
detach, then proves the importer rejects the snapshot, closes the transaction,
detaches the schema, preserves the pre-existing UTXO, leaves outputs untouched
and remains able to complete a subsequent valid import. The cleanup extraction
reduced the importer complexity pin from M=50 to M=48.

The focused normal and ASan/UBSan groups pass, as do `lint-fast`, architecture,
consensus-parity, sealed-core and the production C23 build. One concurrent
normal-harness launch received the build system's explicit unverified-epoch
retry signal while another build lease was active; the prescribed rerun after
that lease completed passed. Consensus impact: NONE. Failure cleanup around a
local snapshot transaction changed; accepted blocks/transactions, serialized
consensus data, chain history, PoW, monetary policy, activation and
cryptographic validation are unchanged. Hetzner's `f261d245d` block-swarm work
does not overlap. Remaining risk: a persistent two-attempt rollback failure is
diagnosed but necessarily leaves SQLite owning the unresolved transaction;
the caller already receives failure and must not promote snapshot authority.
Recommended next investigation: make metadata/block/count reads distinguish
SQLite errors from absent rows and verify source-close errors are observable.

## 2026-09-20: atomic snapshot UTXO anchor publication

The importer committed the replacement UTXO set before writing the
`coins_best_block` projection. An authorizer fault that denied only that state
write established the red baseline: the importer returned failure and retained
the prior anchor, but all 1,200 snapshot UTXOs had already replaced the single
pre-existing UTXO. Retrying or resuming from that torn pair could reason about
coins from one generation under an anchor from another.

The anchor projection write now executes inside the same transaction as the
UTXO delete/copy and before the checked commit. If it fails, the shared cleanup
path rolls back both mutations. The regression plants a prior UTXO and anchor,
denies the anchor write, and proves both survive unchanged; the subsequent
normal import still publishes the fixture UTXOs and expected tip together.
Removing the post-commit restoration branch also reduced importer complexity
from M=48 to M=47.

The focused normal and ASan/UBSan harnesses, `lint-fast`, architecture,
consensus-parity, sealed-core and production C23 build gates pass. Consensus
impact: NONE. This changes atomic publication of local rebuildable snapshot
state only; chain history, block and
transaction validity, wire/consensus serialization, PoW, monetary policy,
activation and cryptographic validation semantics are unchanged. Hetzner's
`f261d245d` block-swarm work remains non-overlapping. Remaining risk: authority
epilogue state spans node.db and consensus.db and cannot share one SQLite
transaction; its failure sentinel/recovery behavior remains the next recovery
surface to fault-inject.

## 2026-09-20: exact node-state blob lengths

The shared `node_db_state_get` bounded-copy API reported the number of bytes it
copied rather than the full stored blob length. A fixed-width reader therefore
could not distinguish an exact record from an oversized corrupt record whose
prefix fit its buffer. The red baseline demonstrated both consequences: a
9-byte state blob was accepted by the 8-byte integer decoder, and a 32-byte
blob read into four bytes was reported as length four.

The getter continues to copy at most the caller's capacity, preserving bounded
text/diagnostic reads, but now reports SQLite's full stored length. Exact-width
decoders consequently reject oversized records while truncating callers can
detect and label truncation. The focused SQLite selection passed all 3 groups
in normal and ASan/UBSan modes. `lint-fast`, the complexity and architecture
gates, consensus parity, sealed core, the regenerated capability inventory,
and the production C23 build all pass.

Consensus impact: NONE. This hardens local node-state decoding and changes no
consensus or wire serialization, validity rules, chain history, PoW, monetary
policy, activation or cryptographic validation. Hetzner's `f261d245d`
block-swarm work remains non-overlapping. Remaining risk: state writers still
cast `size_t` blob lengths to SQLite's signed `int` interface without an
explicit `INT_MAX` bound and do not consistently check bind/finalize results.
Recommended next investigation: harden the primary and detached state writers
with deterministic invalid-input and SQLite-fault coverage.

## 2026-09-20: checked node-state writers

Both node-state writers narrowed `size_t` lengths to SQLite's signed `int` and
discarded key/value bind results. Because the value column permits SQL NULL, a
failed oversized blob bind could leave the parameter NULL, step successfully,
and return a false persistence success. A deterministic connection length
limit reproduced that behavior: the pre-fix writer returned true for a
256-byte value rejected by `sqlite3_bind_blob`, leaving a NULL row.

The primary and detached writers now share bounded argument validation and one
bind/step/finalize helper. Lengths above `INT_MAX`, null/closed handles and null
key/value pointers fail before SQLite; bind errors are preserved; successful
steps still require successful finalization. The regression proves a bind-time
`SQLITE_TOOBIG` returns false without a readable row and separately proves the
oversized public length is rejected without dereferencing its one-byte test
buffer. Extracting the shared path reduced `node_db_state_set_detached` from
M=18 to M=14, allowing its complexity exception to be removed entirely.

The focused `test_sqlite` group passes in normal and ASan/UBSan modes. The
lint, architecture, generated-inventory, consensus-parity, sealed-core and
production-build checks also pass. Consensus impact: NONE. This changes local
projection/error handling only; chain history, validity, consensus/wire
serialization, PoW, monetary policy, activation and
cryptographic validation are unchanged. Hetzner's `f261d245d` block-swarm work
does not overlap. Remaining risk: state reads and deletes still discard bind or
finalize errors, so the same checked lifecycle should be extended there with
focused fault coverage.

## 2026-09-20: checked node-state reads and deletes

The remaining node-state operations discarded key-bind and finalization
results. With a persisted 199-byte key and SQLite's connection length limit
lowered to 128 bytes, the red baseline showed the read converting
`SQLITE_TOOBIG` into an ordinary missing-key `SQLITE_DONE`, while delete
returned success even though the requested row remained present.

Reads now validate their handle/key/buffer contract, preserve bind errors in
the connection health status, log non-missing lookup failures, and require
successful statement finalization before publishing the stored length.
Deletes now preserve bind and finalize failures and only report success after a
completed statement. The focused regression proves `SQLITE_TOOBIG` remains
observable, delete fails rather than claiming removal, and the row remains
readable once the injected limit is restored. Missing keys continue to be a
quiet false read, and deleting an actually absent valid key remains successful.

The focused normal and ASan/UBSan harnesses, lint, complexity, architecture,
generated-inventory, consensus-parity, sealed-core and production-build checks
pass. Consensus impact: NONE. This is local projection error handling only and
changes no validity, chain history,
wire/consensus serialization, PoW, monetary policy, activation or
cryptographic validation. Hetzner's `f261d245d` block-swarm work is unaffected.
Remaining risk: empty node-state blobs are still represented as unreadable by
the historical API contract; changing that behavior would require a separate
caller audit rather than being folded into error-lifecycle hardening.

## 2026-09-20: resumable snapshot authority epilogue

Snapshot UTXOs and their node.db anchor must commit before the separate
consensus.db authority epilogue can run. On an epilogue failure, both startup
callers nevertheless saw more than 1,000 node.db UTXOs on the next attempt and
skipped the importer permanently. A deterministic denial of the coins-store
reset established the red baseline: import returned false after committing the
1,200-row node.db set, but left no durable indication that consensus authority
was incomplete.

The node.db import transaction now publishes a one-byte pending receipt with
the UTXO set and anchor. Only a fully successful authority epilogue clears it.
Both pre-restore and late service import paths consult the checked receipt and
may use their existing-UTXO shortcut only when no receipt is pending; an
unreadable receipt also forces retry. The regression denies the epilogue reset,
proves the receipt survives and suppresses the shortcut, removes the fault,
re-runs the real importer, and proves successful recovery clears the receipt
and re-enables the shortcut.

The focused normal and ASan/UBSan harnesses, generated-inventory check, lint,
silent-error and complexity ratchets, architecture and consensus-parity gates,
sealed-core check, and production C23 build pass. Consensus impact: NONE. The receipt
coordinates recovery of local rebuildable databases only; validation,
consensus/wire serialization, chain history, PoW, monetary policy, activation
and cryptographic semantics are unchanged. Hetzner's `f261d245d` block-swarm
work remains non-overlapping. Remaining risk: if a pending receipt survives but
the snapshot artifact is removed before restart, the current boot path cannot
resume it; a subsequent slice should explicitly gate that missing-artifact
case rather than silently treating the node.db mirror as complete.

## 2026-09-20: artifact-independent snapshot recovery

The first durable receipt prevented the existing-UTXO shortcut after an
authority-epilogue failure, but its one-byte payload could not finish recovery
without reopening the original snapshot. The measured restart reproduction
denied the coins reset after the 1,200-row node.db commit, removed the snapshot
artifact, and then failed the only available retry at `stat(2)`; the receipt
remained pending indefinitely.

The receipt now stores a version, bounded snapshot height, and best-block hash
in a fixed 41-byte little-endian record inside the same transaction as the UTXO
generation and `coins_best_block`. Before chain restoration, boot validates the
record length/version, requires at least 1,000 installed UTXOs, byte-matches its
hash to the installed anchor, and completes the shared authority epilogue
directly from node.db. Missing, malformed, mismatched, or unreadable recovery
state fails boot closed. Snapshot heights that cannot safely form the epilogue's
signed 32-bit next-height cursor are rejected before import.

The focused regression removes the artifact after the injected epilogue
failure, resumes successfully, verifies the exact UTXO count, height and hash,
and proves the receipt clears only after completion. A one-byte malformed
receipt remains pending and is rejected. The focused normal and ASan/UBSan
harnesses, generated-inventory check, fast lint, file-size/flag, complexity and
silent-error ratchets, architecture and consensus-parity gates, sealed-core
check, and production C23 build pass. Consensus impact: NONE. This is local recovery
metadata around rebuildable node/progress databases and changes no validation,
chain history, consensus/wire serialization, PoW, monetary policy, activation,
or cryptographic semantics. Hetzner's `f261d245d` block-swarm work remains
non-overlapping. Remaining risk: receipt removal follows the cross-database
epilogue and is not atomic with it; its safe failure mode is an idempotent
epilogue replay on the next boot.

## 2026-09-20: checked UTXO counts before destructive recovery

`node_db_utxo_count()` returned zero for invalid handles and every SQLite
prepare, step, or finalize failure. A deterministic authorizer denial of the
`utxos` read reproduced the ambiguity: the legacy API reported the same value
as an honestly empty table. More seriously, `node_db_wipe_utxos()` consumed
that value and could proceed with destructive cleanup without ever establishing
how many rows existed.

The database model now provides `node_db_utxo_count_checked()`, which validates
its arguments, requires the aggregate row and terminal `SQLITE_DONE`, checks
finalization, logs the concrete SQLite failure, and publishes its output only
on success. The compatibility projection retains its historical integer return
for diagnostic callers but no longer fails silently. UTXO wipe now refuses to
run after a count failure, and snapshot receipt recovery uses the checked count
before trusting its installed-row floor.

The focused SQLite fault regression proves an authorization failure is
observable, leaves the caller's output untouched, and blocks the wipe; after
the fault is removed, a real empty count succeeds. The broad SQLite selection
and snapshot boot regression pass in normal and ASan/UBSan modes, as do the
complexity and silent-error ratchets. Remaining repository gates are recorded before commit.
Consensus impact: NONE. This changes local database error propagation only;
validity, chain history, consensus/wire serialization, PoW, monetary policy,
activation and cryptographic semantics are unchanged. Hetzner's `f261d245d`
block-swarm work remains non-overlapping. Remaining risk: other recovery
decisions still consume the compatibility projection and should migrate to the
checked API in focused, fault-injected slices.
