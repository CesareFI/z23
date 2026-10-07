# Durable change-index storage

The C Android/Linux storage adapter supports fresh paired wallet/state creation,
bounded state observation and compare-and-append. It reuses the existing private
directory and nonblocking `.lock` lifecycle. It never overwrites, truncates,
deletes, compacts, or silently initializes an existing wallet's change state.

This is public-data IO. The caller must first authenticate the exact wallet
record with hardware/GCM, verify its recovered entropy, and authenticate state
records with the [change-state codec](CHANGE_STATE.md). Storage checks the
record's structure and file position; it cannot check a MAC without the secret
codec. No index, address, signing authority or transaction approval is returned
by these IO functions. The C reservation wrapper below supplies recovered-wallet
and MAC sequencing. There is no JNI or Android sending integration yet.

## Fixed append log

The private `.change.index` file contains fixed80-byte authenticated records.
A complete record's next-index value must equal its zero-based file position.
Initial record0 contains next_index0. Reserving indexN requires appending record
N+1 and completing all durability/cleanup checks before publication elsewhere.

The cap is65536 records, 5,242,880 bytes (5MiB). The adapter can complete65535
reservations, indexes0..65534, before refusing further growth. This conservative
storage cap is smaller than the cryptographic derivation domain. No automatic
rollover, reuse or compaction is provided. Unused consumed indexes remain gaps;
seed restoration must explicitly account for them before sending is enabled.

`zcl_change_storage_snapshot` owns a uint32 file size, a tail length and80 bytes.
Observation reads only the final min(size,80) bytes. It may describe an empty or
partial existing file for explicit recovery; that observation cannot authorize
a normal append. No scan, allocation, retained descriptor or growing collection
is used. Snapshot bytes are unauthenticated until the caller verifies them.

## Fresh creation and recovery boundaries

`zcl_storage_create_with_change` requires committed wallet, pending wallet and
change file all absent under one lock. Its order is:

1. Exclusive-create initial state0; complete its write, fsync and close.
2. Fsync the directory so the initial state name is durable.
3. Use the existing pending-wallet write/fsync lifecycle.
4. Fsync the directory, perform no-replace rename, and fsync the directory again.
5. Close lock and directory before reporting success.

An incomplete state file, or a complete initial state whose wallet creation did
not start, remains an explicit recovery condition. It cannot be replaced by
another fresh wallet, including through the older receive-only create API.
If a complete pending wallet exists, the existing platform GCM/recovered-wallet
validation and promotion remain required. Paired creation ensures initial state
was durable before any pending wallet write started.

An existing committed wallet with no change file returns NOT_FOUND from
observation and append. It is never treated as an unused wallet or reset to0.
Old receive-only wallets need a separate authenticated migration and discovery
decision. Explicit repair IO is available below; its authenticating recovery
caller is still required. Normal reservation preserves damaged bytes and refuses.

## Comparison, durability and failure

Observe and append match all supplied authenticated wallet-record bytes against
committed `wallet.zcl` under the same directory lock. Pending records cannot be
used until promoted. A mismatch returns ALREADY_EXISTS. The change file must be
regular, owned by the effective UID, private to that UID, single-linked and
within the size cap. Opens use nofollow/nonblock/cloexec; append adds RDWR/APPEND.

Append reads and validates the same descriptor it later writes. It compares the
exact observed size/tail, requires a nonempty aligned log and validates old/new
structural counters against their positions. A stale observation returns BUSY;
there is no internal retry. Caller authentication must reject any invalid MAC.
Each read/write loop has at most256 attempts and fsync has at most16. Positive
short counts advance only after bounds checks; persistent EINTR refuses.

The complete append is followed by file fsync, file close, directory fsync,
then lock/directory close. Any uncertain write, flush or close refuses success.
Linux/Android close is consumed once even on EINTR; retrying could close a
reused descriptor. Observe publishes its snapshot only after all closes succeed.

| Observed result after interruption | Required next behavior |
| --- | --- |
| No appended bytes | Old authenticated head may be reserved through a new checked call. |
| Partial appended record | Preserve bytes; normal append refuses; explicit authenticated recovery required. |
| Complete appended record, even if the call failed | Prior index is consumed; stale snapshot refuses and the next append advances again. |
| Missing state for a committed wallet | Refuse; no automatic initialization. |
| Cap reached | Refuse further growth; no rollover. |

## Explicit repair IO

`zcl_storage_change_repair` is a separate public-data IO operation. It is never
called automatically by normal reservation. Before invoking it, the recovery
caller must verify recovered-wallet identity, classify damage and authenticate
the replacement record. Healthy or authenticated-but-misplaced heads must refuse
repair. That authenticating recovery caller remains unfinished; no JNI route
exposes this primitive.

Under the existing lock and on the same append descriptor, repair compares the
exact observed size/tail and wallet bytes. It appends only zero padding needed
to finish the current80-byte slot, then a newly authenticated record at the next
position. Existing empty files pad one full slot and append record1, consuming
at least index0. Missing files still refuse; no file is created by repair.

| Existing damaged bytes | Zero padding | Replacement counter | Final bytes |
| --- | --- | --- | --- |
| 0 | 80 | 1 | 160 |
| 40 | 40 | 1 | 160 |
| 80 | 0 | 1 | 160 |
| 120 | 40 | 2 | 240 |
| 200 | 40 | 3 | 320 |

The checked plan permits at most80 padding bytes and160 total new bytes, within
the existing cap. No previous byte is changed. The counter is the new record's
file position, so padding and uncertain attempts conservatively consume gaps.
Capacity checks precede writes; no truncation, replacing rename, reset or
rollover is available. A repair at the last available slot leaves an exhausted
head, without creating more reservation capacity.

Failure during padding or replacement preserves all bytes written so far and
returns no success. A later explicit attempt computes its plan from the larger
observed size, preserves prior padding/partial replacement too, and advances
again where needed. Complete uncertain replacements remain stored. The same
file flush/close, directory flush and owner cleanup precede success. Repair
returns no address or reservation; normal authenticated reservation is separate.

Deterministic tests preserve every prefix length0..159, exercise final capacity,
missing-state and stale/wrong-wallet refusal, and verify subsequent authenticated
reservation in representative cases. Faults cover both padding and replacement,
partial completion, every flush/close and descriptor counts. Twelve reached
child-process interruption boundaries verify byte preservation across repeated
repair. These IO fixtures do not qualify the pending damage-classification or
authentication policy and do not simulate power loss.

File length alone is not proof of a consumed-index bound after data loss. The
authenticating recovery caller must establish that bound from verified records
before allowing this IO. Empty/short initial state with a committed wallet
contradicts fresh paired-creation ordering and may represent lost history;
padding capability does not authorize its use in that case. Recovery of a
recognized interrupted suffix needs an authenticated predecessor, supported
record profile and consistent position. Ambiguous loss or unsupported formats
must remain preserved and refused until independent recovery evidence exists.

## Bounded predecessor probe

`zcl_storage_change_probe` returns owned public recovery metadata: the current
size/tail, a predecessor-presence flag and the complete80-byte record immediately
before the current complete or partial slot. Files of at most80 bytes have no
predecessor, and that array stays zero. This observation is unauthenticated and
cannot itself establish consumed indexes or approve repair.

The probe matches the same committed wallet under the existing lock, reads tail
and predecessor on one descriptor, rechecks file metadata/size, and publishes
only after all descriptor cleanup succeeds. It reads at most160 state bytes,
without scanning history or retaining pointers. Normal observation still reads
only the current tail. Both paths share one explicitly bounded pread helper.

Tests check every byte length0..320, the file-cap offsets, actual authenticated
predecessors and partial successors. Faults cover both reads, persistent EINTR,
all metadata/close stages, descriptor counts and a size change before publication.
The authenticating recovery caller must still verify MAC, profile and position,
and refuse missing/unverifiable predecessor or ambiguous loss. No repair or
address publication is introduced by probing.

## Authenticated C reservation

`zcl_wallet_change_create/reserve` in `zcl_change_reservation.h` compose the
recovered-wallet codec, derivation and IO. The platform must first authenticate
the exact ciphertext/header/entropy with GCM and its per-use hardware policy;
C cannot prove that platform action. These operations belong on a worker thread.

C copies the bounded encrypted wallet record before parsing it. Every following
crypto and storage operation uses that same private copy. Entropy stays borrowed
for the synchronous call, must remain stable and must be cleared by its caller.
There is no retained seed, MAC key, descriptor or authorization handle. OS
blinding is generated internally and clears after each operation, even when a
test-injected RNG or cryptographic failure leaves partial scratch output.

Fresh create verifies recovered-wallet identity and encodes state0 before
paired creation. The caller cannot choose an initial counter. Wrong entropy,
RNG failure or codec failure cannot create wallet/state files. Existing/orphan
state remains a refusal; this API does not provide migration or repair.

Reserve observes the exact committed wallet/state, validates a complete bounded
head, authenticates its MAC and recovered wallet, and requires the verified
index to equal the file position. It privately derives the internal-chain
address with independent blinding, encodes the authenticated successor, then
performs the checked append. Only after all durability and descriptor cleanup
succeed does it publish the public index, network and35-byte canonical address.
It never takes a caller-selected index or state record.

No error changes the caller's result, including when a complete append occurred
before a late failure. Such an index is consumed; the next reservation advances.
A competing append after observation causes BUSY and no publication. Missing,
partial, bad-MAC, misplaced or exhausted state refuses without automatic repair.
Cancelling or losing a result after success also burns its index. No decrement,
retry, secret cache or address reuse is introduced to hide these outcomes.

The public reservation result is not a transaction approval or change-output
authentication receipt. The sending flow still needs exact review/authorization
binding, cancellation/lifecycle checks and recovery/discovery acceptance. No
signing/broadcast or hardware-policy relaxation is supplied here.

## Android fresh creation

Fresh Android creation now reaches paired persistence through
`WalletStorage.createFreshWithChange`. JNI copies the bounded path, encrypted
record and entropy into invocation-private arrays, calls the C creation owner
only after all three reads succeed, and clears its entire 32-byte entropy
scratch on every exit. Scalar status publication performs no VM allocation.
Pending Java exceptions remain pending and prevent further array access.

`WalletPlatformSession` chooses this route only for CREATE after its existing
per-use GCM encryption and backup-confirmation flow. RESTORE retains wallet-only
creation because a recovered seed can have historical consumed change indexes;
it cannot initialize index0 without discovery. UNLOCK cannot enter either
creation route. No existing wallet, pending record or orphan state is replaced.
The platform worker still owns and clears managed entropy, and foreground/session
closure still suppresses late UI delivery. An in-flight persistence operation
may finish after closure; its preserved records require ordinary restart
inspection rather than another creation/reset attempt.

The JNI fault fixture observes live entropy clearing after every array-access
failure, including a partial secret read, pending exception, invalid argument,
core refusal and successful creation. JVM tests exercise both networks/all
entropy widths and actual action-dependent files. The Android public-provider
GCM fixture exercises paired creation and wallet-only restoration separately.
These tests do not qualify positive hardware custody on the software emulator.

## Authenticated suffix recovery

`zcl_wallet_change_recover` is an explicit synchronous operation with the same
exact-record GCM, per-use hardware and stable caller-span requirements as
reservation. It returns no address, index or authorization handle. Normal
reservation never invokes it as a fallback.

Recovery first checks the current tail. A valid MAC must also have the correct
aligned file position. A healthy head causes no write and returns ALREADY_EXISTS;
an authenticated but misplaced head refuses. ALREADY_EXISTS can also report an
exact-wallet storage conflict, so that status alone is not proof of healthy
wallet state or a new durability guarantee. Only OK reports a completed repair.
RNG, provider, IO and internal failures never grant permission to repair.

An unverified tail can proceed only when its immediately preceding complete
record authenticates against the recovered wallet at the exact expected
position. The current suffix must include all 16 public prefix bytes matching
the supported v1 format and expected counter. This comparison uses the existing
codec to produce the expected prefix; MAC comparisons remain in that codec.
Recovery then authenticates a new successor and calls the append-only repair
primitive with the original observation and private wallet copy. Its bounded
plan and compare-and-append checks still apply.

Missing, empty, short initial, unsupported, unrecognized or exhausted state
refuses without modification. Recovery never treats file length alone as
permission to initialize a consumed-index bound. It does not scan older records,
roll back, migrate formats or relax the immediate-predecessor requirement.

Failure preserves all bytes, including newly written padding and any partial
replacement. A complete successor consumes the skipped indexes even if a later
flush/close reports failure. An incomplete replacement may now have an invalid
immediate predecessor: another authenticated recovery attempt must refuse it.
Such ambiguous state needs independent discovery or security review. The raw
IO primitive's ability to append again is not authority for this caller to do
so. No internal retry or automatic recovery loop is supplied.

Deterministic tests cover every original length 0..159, header/tag corruption,
authenticated wrong-position predecessors and heads, a misaligned valid tail,
both networks/all entropy widths, exact wallet binding and capacity. Successful
repairs preserve the entire original prefix and support subsequent reservation
at the authenticated successor. Source-only faults cover all four RNG/codec
steps, live blinding cleanup, competing repair and every close/flush stage.
Eighteen reached child-process interruption boundaries check preserved bytes,
complete-successor consumption and refusal of ambiguous partial replacement.
These fixtures use public entropy and inert ciphertext, and do not simulate
power loss, establish GCM/hardware custody or qualify seed discovery.

## Threat and evidence limits

The lock coordinates cooperating processes. A same-UID attacker who replaces,
truncates, or restores an older valid filesystem snapshot can defeat freshness;
MACs prove content, not recency. This adapter does not claim malicious rollback
resistance, authenticated chain discovery, unspent funding, or seed-restoration
completeness. The caller supplies a trusted app-private path and stable spans.

Deterministic local fixtures cover no-overwrite, exact wallet/CAS binding,
partial/empty files, bounded tail reads, cap/length extremes and file policy.
Injected short/EINTR/failed IO, partial writes, every flush/close stage and
descriptor counts test cleanup and publication refusal. Child processes stop
at26 reached write/flush/rename/close boundaries; twelve competing appenders
must produce exactly one success. These are process/IO observations, not a
simulation of filesystem power loss or a hardware-custody qualification.

Reservation fixtures cover both networks/all five entropy widths, re-derived
public addresses, authenticated successor records, wrong entropy/wallet/MAC,
all short initial files, final permitted index65534 and exhaustion. Source-only
hooks fail each RNG/decode/derive/encode step, verify live blinding cleanup,
change the original ciphertext after its private copy, and simulate a competing
append. IO faults check unchanged public results and conservative consumption
after late failures. Inert public ciphertext fixtures make no GCM claim.
