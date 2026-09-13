# Authenticated change counter record

The C `zcl_change_state_encode/decode` API authenticates one internal-chain
counter record against recovered wallet entropy and its exact version1 wallet
header. It is a bounded content codec. It performs no storage operation,
reservation, increment, freshness check, signing or transaction approval.

The platform must first authenticate GCM with the exact wallet header and
ciphertext. Each call reuses `zcl_wallet_recovered_address` to verify entropy
length, profile and independently derived first receiving address. C cannot
prove that hardware/GCM authentication occurred. The caller supplies fresh
32-byte OS-random blinding for that existing wallet check and clears its secret
spans afterward. No key, entropy pointer or authentication handle is retained.

## Exact format

| Offset | Bytes | Meaning |
| --- | --- | --- |
| 0 | 4 | ASCII `ZCLI` |
| 4 | 1 | Version1 |
| 5 | 1 | Internal-chain profile1 |
| 6 | 2 | Zero, reserved |
| 8 | 4 | Next index, uint32 little-endian |
| 12 | 4 | Zero, reserved |
| 16 | 64 | Full HMAC-SHA512 tag over the first16 bytes |

The complete record is exactly80 bytes. Every reserved byte, version/profile,
length and counter range is checked. The next index is0..0x80000000, inclusive;
0x80000000 means exhausted and cannot be passed to address derivation. A
successful encode writes exactly80 bytes. Decode publishes only one uint32
counter after all checks, including authentication. Outputs remain unchanged
on any failure. Input/output spans must be stable and nonoverlapping.

## Private MAC key

The existing bounded HMAC-SHA512 helper implements a fixed
[HKDF extract/expand profile](https://www.rfc-editor.org/rfc/rfc5869), deriving
one64-byte block. The input is uniformly generated recovered wallet entropy,
not a password. Extract uses the fixed independent ASCII salt
`Zclassic Android change state v1`, excluding its terminator. Expand uses
`authenticated index record` (also without a terminator), followed by all80
wallet-header bytes as its context. The first HKDF block appends byte01. This
binds the MAC key to the exact network/genesis/recovery profile/root address.

The full tag authenticates the record's first16 bytes. Extracted and derived
keys clear on every exit, including partial-output fault injection. No private
MAC key is part of the public codec API or a JNI result. The tag is compared
through a volatile accumulation over all64 bytes. Inspected Clang20 x86_64 and
NDK ARM64 `-O2` assembly read every byte before deciding equality; this is
evidence for those builds, not a universal compiler/hardware timing proof.

The counter and its tag are public metadata, without encryption. Authentication
does not establish when the record was written. An older valid record still
authenticates, which is explicitly tested. A file owner must separately enforce
which record/index may be consumed next. No codec result alone authorizes a
change label, a transaction or an index reset.

## Required persistence and recovery work

The immutable wallet record remains unchanged. There is no automatic counter
initialization, upgrade, or reset for existing wallets. Before using change in
a sending flow, the storage owner must enforce:

- Explicit authenticated initialization and bounded monotonic reservations.
- Complete write and durability checks before an index/address is returned.
- Conservative consumption after uncertain writes or interrupted publication.
- Preservation and explicit recovery of incomplete/corrupt state.
- Refusal to treat missing/corrupt data as permission to restart at zero.
- Process-crash and IO-fault evidence for every publication boundary.
- An explicit filesystem-rollback threat boundary; MACs do not supply freshness.
- Seed-restoration discovery that accounts for consumed but unused indexes.

These remain active development requirements. No JNI or Android sending path
uses this codec yet. TLS and BLAKE2 quarantine, hardware policy and the existing
wallet record/storage format remain unchanged.

## Evidence scope

Independent host OpenSSL3.0.13 HKDF/HMAC produced matching complete records for
40 network/entropy-width/counter combinations. That oracle takes a public wallet
header as context and does not requalify address derivation. Deterministic tests
cover all640 single-bit record edits, truncations, changed headers, another
wallet/network, invalid bounds, sentinel handling, canaries and old valid
content. Separate faults at extract, expand and MAC production observe live
key cleanup and unchanged caller outputs. OpenSSL and these fixtures are
host-only and never linked into the Android app.
