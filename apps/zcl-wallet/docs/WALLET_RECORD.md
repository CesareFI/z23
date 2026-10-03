# Version 1 authenticated wallet record

The wrapping primitive is Android Keystore AES-256-GCM, with a provider-generated
12-byte IV and a 16-byte authentication tag. The C core owns the serialized
record and recovery profile. No plaintext secret is written to a file.

| Header offset | Bytes | Meaning |
| --- | --- | --- |
| 0 | 4 | ASCII `ZCLW` |
| 4 | 1 | Record version 1 |
| 5 | 1 | Network: 0 mainnet, 1 testnet |
| 6 | 1 | Recovery profile 1: BIP39 English, empty passphrase, BIP44 Zclassic account 0, external chain, first address |
| 7 | 1 | Entropy length: 16, 20, 24, 28 or 32 |
| 8 | 4 | Receiving index, little-endian; version 1 requires zero |
| 12 | 32 | Network genesis hash in displayed big-endian byte order |
| 44 | 35 | Transparent P2PKH receiving address in ASCII |
| 79 | 1 | Reserved, must be zero |

All 80 header bytes are GCM authenticated additional data. The file is the
header, IV, then encrypted entropy with appended tag, for exactly 124..140 bytes.
No native structure or padding is serialized. Parsers reject extra/truncated
bytes, unknown versions/profiles, wrong genesis/network, nonzero reserved fields,
invalid entropy sizes and invalid receiving addresses before asking Keystore to
decrypt. Parsing alone establishes no authenticity.

The C packer validates every span and capacity before copying directly into the
caller-owned record, avoiding a second ciphertext array. Its metadata scratch
and the parser's staged record are securely cleared before return, including
refusals after initialization. JNI separately clears its native copies after
publication or VM failure. Host fixtures observe complete scratch erasure while
the objects are live and require independent, unchanged caller-owned results.
These are ciphertext/metadata lifetime guarantees, not GCM authentication or a
claim that every managed runtime/provider copy can be erased.

The receiving address must not be displayed from unauthenticated file metadata.
After authenticated decryption, the C core checks entropy length and independently
derives the first receiving address again. Only a match permits the UI to expose
that address. The entropy is then cleared. A process restart requires this check
again. A future receive-index/discovery format needs explicit versioned behavior.

The storage interface accepts only this bounded ciphertext format. Creation must
refuse to replace an existing wallet. A pending file supports interrupted setup;
only a complete record that has been decrypted and re-derived may be promoted.
Truncated/corrupt data is a recovery condition, never permission to generate a
replacement wallet silently. No erase/overwrite operation is part of version 1.

This format authenticates content, not freshness. It does not claim protection
against restoration of an older valid record by an attacker with filesystem
control. Version1 `wallet.zcl` remains immutable. Separate mutable state needs
its own rollback and recovery design.

The [change-counter codec](CHANGE_STATE.md) now authenticates a separate fixed
record against this recovered wallet identity. The public-data
[change storage adapter](CHANGE_STORAGE.md) provides fresh paired creation and
bounded compare-and-append. The C reservation wrapper now authenticates state,
privately derives change and publishes only after durable append; neither codec
nor IO alone grants index/address publication. Explicit repair, migration,
discovery and Android sending integration remain unfinished.
Both creation APIs refuse an existing change file, including orphan state left
by interrupted paired creation. Missing state never permits an existing wallet
to initialize an index automatically.
Wallet reads return `NOT_FOUND` only when committed, pending and change names
are all absent. If both wallet records are absent but a change entry remains,
the read returns `ALREADY_EXISTS` without publishing outputs or reading that
entry. Android therefore takes the existing storage-failure path before setup.
The locked metadata check also refuses dangling symlinks and FIFOs promptly;
it does not authenticate, repair or remove orphan state.

## Recovered-wallet internal address binding

`zcl_wallet_recovered_change` checks the same version1 header/recovered entropy
agreement as receiving-address recovery before deriving an internal chain1
address at the explicitly requested index. The selected network/account/profile
comes from that checked header. A caller cannot supply a separate network or
use another wallet's header with the recovered entropy. The result is exactly
35 public ASCII bytes, without a terminator, and remains unchanged on failure.

The platform must authenticate GCM with the exact header and ciphertext before
calling this function. C establishes recovery-profile consistency; it cannot
observe or substitute for hardware authentication. The caller supplies two
independent32-byte OS-random blinding values in one64-byte span, one for the
wallet check and one for internal derivation. Both operations reuse existing
bounded key derivation/cleanup. Secret spans remain caller-owned and must clear
after use; no private key, seed, pointer or context is returned or retained.

Only indexes below2^31 are accepted, and invalid children refuse without an
implicit retry. This adds no mutable record field or JNI entry. It does not
reserve an index, classify a transaction output as change, establish funding,
or grant transaction authorization. Durable index reservation, cancellation
and seed-restoration discovery remain separate requirements before using a
change output in a sending flow. The immutable version1 wallet is unchanged.

## Storage implementation

`zcl_storage_*` is the Android/Linux C filesystem adapter. It accepts an absolute
private path from the platform with at most 1024 bytes, rejects embedded NULs,
empty/dot/parent components and trailing slashes, and creates only the final
directory component. Ancestors must be trusted platform-owned directories;
this is not a general untrusted-path sandbox. The opened final directory must
belong to the effective user and deny group/other permissions. The parent is
flushed so a newly created child directory is durable before success is reported.

The C call owns its directory, lock and record descriptors. Every descriptor
has one close path; `close` is never retried on Linux/Android after EINTR because
the number may already have been released. There are no retained descriptors,
native handles, C heap allocations or mutable global state in this adapter.
Private regular files and `O_NOFOLLOW` prevent symlink/device/FIFO substitution
at the final file components; nonblocking opens prevent a FIFO from stalling
before its type check. Size is checked before reading and EOF checked afterward.

The fixed `.lock` file uses nonblocking `flock`. Cooperative callers receive
`BUSY` rather than wait. This does not defend against an already compromised
process running with the application's UID that ignores locks or rewrites
private files. GCM authentication remains necessary even when the filesystem
checks pass. Stored public metadata also reveals the receiving address to an
attacker who obtains the record; GCM authenticates that metadata, not hides it.

Creation writes `.wallet.pending` exclusively, checks bounded short writes,
flushes its contents and directory entry, then uses
`renameat2(..., RENAME_NOREPLACE)` to move it to `wallet.zcl` atomically.
The directory is flushed again before acknowledging the commit. There is no
mutation or replacement of an existing committed record and no replacing-rename
fallback on an unsupported filesystem. The NDK exposes this operation from
Android API 30, the application's minimum version.
Promotion compares the exact authenticated bytes under the lock and flushes
the recovered file before renaming. An idempotent retry on an identical committed
record re-flushes its contents and directory before reporting success. Committed
data takes precedence over any unrelated pending file, and no unauthenticated
cleanup is attempted.

Temporary read and exact-record comparison arrays are securely cleared on
success and refusal, including IO and capacity failures. Validation-only parsed
records are retired before subsequent filesystem work. Promotion finishes its
byte comparison and retires that copy before the commit/directory flush; the
caller still owns the authenticated input and every published output.

JNI storage additionally retires its full copied path and ciphertext record on
every create/promote/fresh return, preserving entropy-first retirement for fresh
creation. Read clears its path before VM allocation and its full native packet
after success or any read/publication exception. Successful VM arrays remain
independent; a partial transfer never returns a successful array. These lifetime
checks do not authenticate ciphertext or erase managed/provider copies.

An IO error after creating the pending file is `IO_UNCERTAIN`; callers must
re-read storage and authenticate any complete record. An incomplete file stays
intact as an explicit recovery condition. Errors never authorize overwriting
it or silently replacing the wallet. A corrupt committed file never falls back
to a pending one. Error paths leave read outputs unchanged. Read/write attempts
are capped at 256 and flush/EOF interruption retries at 16. These bound retry
loops, not kernel latency; filesystem calls still run off the Android UI thread.

Tests cover every record size/network, malformed fields, every single-bit header
edit, output canaries, truncated/oversized files, privacy/type/lock checks, bounded
IO fault injection, failed closes, and process termination at five commit stages.
Twelve competing processes must produce exactly one successful creator. These
tests observe process/filesystem behavior; they do not simulate physical power
loss or qualify every device filesystem. The Android instrumentation test uses
a public test AES key to check GCM/AAD and native storage on an emulator. It does
not qualify hardware Keystore custody; positive hardware acceptance remains open.

Eight additional native Android storage cases pass on API30, API35 and API36.
Each exclusively creates a temporary private root, uses a public AES-GCM record
and deletes only its fixed invocation-owned entries without following links.
They check exact authenticated pending promotion, mismatch refusal, idempotent
committed retries, no replacement of a different record, corrupt-committed
precedence and preservation of truncated/oversized pending files. A complete
pending record with a bad tag remains structurally readable but fails provider
authentication; the fixture does not promote it or overwrite it by creation.

Real final-directory/file symlinks, group/other permission bits and a nonempty
lock all refuse under native policy. A subsequent valid call still works after
the fixture repairs its own deliberately unsafe metadata. `/proc/self/fd`
checks find no references to the fixture root or its children after synchronous
calls. Four concurrently released Android creator threads produce exactly one
complete record; other results are BUSY or ALREADY_EXISTS, and the winner's
exact bytes are retained with no pending file. These observations qualify the
stated local filesystem/JNI contracts, not malicious same-UID rollback,
physical power loss, production wallet recovery or hardware authentication.

Fresh Android creation additionally persists authenticated change state before
the wallet commit through the existing paired C storage owner. Restoration
deliberately creates no initial change counter: historical index consumption
requires independent discovery. JNI uses bounded private copies and clears its
native entropy buffer on every exit; the platform worker retains its existing
managed cleanup and per-use hardware requirements. See
[change storage](CHANGE_STORAGE.md) for ordering, interruption and refusal rules.

The host JNI key fixture also exercises header creation and recovered-address
derivation through every VM read/allocation/publication fault, pending and NULL
arguments, partial RNG failure, malformed headers and mismatched entropy. It
checks caller input preservation and clearing of all touched entropy/blinding
spans before their lifetimes end. Removing either clear from either entry is
detected by separately compiled negative fixtures. This proves the stated JNI
cleanup contract on public host inputs; it does not authenticate GCM or qualify
hardware custody. Object-array record unpacking has a separate JNI boundary.

The separate host `wallet_jni_record` fixture covers pack/unpack through all
eight/sixteen ordinary VM call ordinals. It checks exact components for both
networks and all five entropy sizes, immutable caller inputs, refusal before
allocation for malformed records, at most two simultaneous local references,
and no returned partial result after an exception. NULL allocations without
exceptions and allocated references with pending exceptions are both injected.
Remaining locals on a refused call belong to the normal JNI return frame;
temporary class/part references are otherwise released promptly. Three negative
fixtures detect missing temporary releases and missing exception refusal.

`NativeRecordInstrumentedTest` separately passes all ten profiles on API30,
API35 and API36 using the real VM and provider GCM. Mutating the parsed arrays leaves the
input record and other arrays unchanged; malformed calls are followed by a
successful authenticated recovery. A structurally valid record with a changed
tag still parses and must then fail provider authentication. This in-memory test
uses public vectors and opens no wallet directory or Keystore alias.

## Verified storage timing tool

From `apps/zcl-wallet`, build and run the explicit Linux host target:

```sh
cmake -S native -B native/build/storage-benchmark-clang \
  -DCMAKE_C_COMPILER=clang-20 -DCMAKE_BUILD_TYPE=Release \
  -DZCL_SANITIZE=OFF -DZCL_FUZZ=OFF -DZCL_TLS_REVIEW=OFF
cmake --build native/build/storage-benchmark-clang --target bench_wallet_storage -j4
timeout 60s native/build/storage-benchmark-clang/bench_wallet_storage
```

Use a separate build directory for GCC or sanitizer observations. The tool takes
no path arguments. It creates exclusively owned temporary stores, uses the
existing public 12-word vector with inert ciphertext, and removes only its own
fixed fixture files/directories. It neither loads a wallet nor claims GCM or
hardware authentication. It is excluded from the default build and introduces
no application/JNI route.

Five profiles measure empty, pending and committed reads, idempotent committed
promotion plus readback, and fresh wallet-only creation plus readback. Every
operation checks status, exact record bytes/length, pending state and unchanged
failure outputs. Each profile has one warm-up batch and five reported batches.
Read/promotion batches perform128 operations across eight stores; creation uses
eight fresh stores exactly once. Fixture preparation, public-vector derivation,
report formatting and cleanup are outside timing. Verification overhead remains
inside. The tool separately reports monotonic wall and thread CPU microseconds.

These are filesystem/cache-dependent observations, not cold-start, flash,
power-loss, paired-change-state, Keystore, GUI or production latency guarantees.
All normal metadata checks, locks, parsing and fsync calls remain enabled. No
performance threshold is part of acceptance; correctness and crash/recovery
fixtures remain authoritative. Exact measured environments/results belong in
`PROGRESS.md`.
