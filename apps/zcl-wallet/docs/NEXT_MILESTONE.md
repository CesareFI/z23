# Next milestone: qualify custody and finish read-only receive

The application remains development-only. No address from a development build
may receive real funds. The C core and Android workflow are implemented; a
successful hardware-authenticated create/restore/unlock flow is not yet proven.

## First acceptance gate

Use a fresh, explicitly authorized test profile on an Android device providing
TEE/StrongBox storage and hardware-enforced per-use authentication. Never point
the fixture cleanup at an existing wallet. The current interactive test fixture
deliberately permits only a fresh opted-in emulator and will need a separately
reviewed real-device fixture boundary. Do not remove its protection checks.

Prove creation with written-backup confirmation, restoration of a public
unfunded vector, authenticated unlock to the exact same receiving address,
denial/cancellation without persistence, background secret-view cleanup,
process recreation, key invalidation and interrupted-record recovery. Repeat
on the oldest supported API and a current supported API. Validate both device
credential and strong-biometric paths where present. Provider metadata and
host tests alone do not establish these outcomes.

The software-only API-35 emulator can exercise refusal and non-custodial JNI/UI
behavior. Its inability to pass the custody policy is an expected refusal, not
permission for a software key or timed-authentication fallback.

## Complete phase 1

The pinned C QR decoder and thin JNI adapter are present; see
[`SCANNING_QR.md`](SCANNING_QR.md). Add the camera permission/lifecycle adapter
with bounded frame ownership and queueing, and a public-request review screen.
Qualify real-camera interoperability, denial, cancellation, backgrounding and
process recreation. Retain no secret scan/import route; a scan must never
authorize a transaction.

Implement bounded read-only networking in C. Define the source trust and
address-privacy model before enabling a real endpoint. Verify the exact Zclassic
chain/network identity, enforce deadlines and byte/count limits, and report
offline/stale/unverified balances explicitly. Exercise malformed responses,
disconnects, retry bounds, reorgs and address/network mismatches against local
fixtures. No production node or mining runs on Worldstream.
The pinned transport-reference findings and original beta6 network differences
are recorded in [`READ_ONLY_SYNC.md`](READ_ONLY_SYNC.md).

Receiving QR encoding is present. Device rendering and real-camera scan
interoperability remain part of this gate; synthetic image decoding is not a
camera acceptance claim.

## Remaining ordered scope

1. Transparent send: C transaction construction, checked money/fee/UTXO rules,
   change ownership, exact pinned-original Zclassic serialization and sighash
   vectors, then signing and explicit user confirmation. No real funds in tests.
2. Shielded transactions: preserve original Zclassic wire/branch/proof semantics;
   qualify parameters, witnesses, anchors, value accounting, recovery and proof
   generation/verification before exposing a send action. No Rust toolchain.
3. Lightweight mobile validation: state precisely which proofs are checked and
   which trust assumptions remain. No consensus shortcuts or block exceptions.
4. Separate encrypted-messaging design: independent identity/key lifecycle,
   authenticated off-chain messages, bounded parsing/queues, replay protection,
   and no access to spending keys, signing or consensus authority.

No phase is considered complete merely because a build or narrow test passes.
