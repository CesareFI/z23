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
not qualify hardware Keystore custody; that platform adapter is the next step.

Fresh Android creation additionally persists authenticated change state before
the wallet commit through the existing paired C storage owner. Restoration
deliberately creates no initial change counter: historical index consumption
requires independent discovery. JNI uses bounded private copies and clears its
native entropy buffer on every exit; the platform worker retains its existing
managed cleanup and per-use hardware requirements. See
[change storage](CHANGE_STORAGE.md) for ordering, interruption and refusal rules.
