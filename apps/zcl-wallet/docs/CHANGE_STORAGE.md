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
by these IO functions. There is no JNI or Android sending integration yet.

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
decision. A recovery implementation for partial/corrupt change files is still
required; this slice preserves those bytes and refuses normal reservation.

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

The future secret-facing reservation wrapper must authenticate the observation,
derive a candidate change address privately, authenticate/encode the successor,
perform this append, and publish only after success. It must preserve the same
wallet binding across those steps. IO success alone cannot replace that wrapper.

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
