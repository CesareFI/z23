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
control. Version 1 creates one immutable wallet record and has no mutable address
index or transaction state to roll back. Future mutable state needs its own
rollback and recovery design.

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
