# Next milestone: qualify custody and authenticated transaction completion

The application remains development-only. No address from a development build
may receive real funds. The C core and Android workflow are implemented; a
successful hardware-authenticated create/restore/unlock flow is not yet proven.
Resume from the implemented baseline below; detailed validation and exact
checkpoint identities are recorded in [`PROGRESS.md`](PROGRESS.md).

## First acceptance gate

Use a fresh, explicitly authorized test profile on an Android device providing
TEE/StrongBox storage and hardware-enforced per-use authentication. Never point
the fixture cleanup at an existing wallet. The original interactive test fixture
deliberately permits only a fresh opted-in emulator; preserve its protection
checks. The separate [attended qualification fixture](DEVICE_QUALIFICATION.md)
now supplies a test-only package/UID and guarded public-vector restore plus two
per-use unlocks. It retains its public wallet/key and refuses existing state.
Its positive hardware execution remains open; emulator refusal and synthetic
admission tests do not substitute for that evidence.

Prove creation with written-backup confirmation, restoration of a public
unfunded vector, authenticated unlock to the exact same receiving address,
denial/cancellation without persistence, background secret-view cleanup,
process recreation, key invalidation and interrupted-record recovery. Repeat
on the oldest supported API and a current supported API. Validate both device
credential and strong-biometric paths where present. Provider metadata and
host tests alone do not establish these outcomes.

API30, API35 and API36 emulator fixtures exercise refusal and non-custodial JNI/UI
behavior. Inability to pass the custody policy is an expected refusal, not
permission for a software key or timed-authentication fallback.

## Complete phase 1

The C QR decoder, bounded camera adapter, isolated decoding service and public
request review are present; see [`SCANNING_QR.md`](SCANNING_QR.md). The API-35
emulator proves separate Binder identity, public request round-trip, permission
refusal, actual captured frames and three background/resume cleanup cycles.
An opted-in image-backed AOSP API35 emulator also passes the full Camera2 to
isolated-decoder to exact-review path, including an explicit fresh scan after
Activity recreation. The normal minified release also passes actual permission
denial, retry, foreground grant, and exact public camera review with worker
cleanup. Background process death also preserves only the selected network
through normal task restoration and requires a new explicit capture, which
decodes the same public request in the replacement process. A real Camera2
callback held on its worker also proves cancellation retains one bounded owner
until callback delivery, rejects a competitor and permits a fresh frame after
cleanup; removing the guard fails this fixture. The oldest supported API30 also
passes the minified permission/camera journey, public native/JNI fixtures,
Keystore refusal and pending-open cancellation. A chooser regression applies
real system-bar insets to a compact viewport; scrolling preserves reachable
network/start/close controls, capture cancellation and public review fields.
Actual camera/recreation and background cleanup tests also pass with that
shared layout. API36 passes eighteen public native, layout, lifecycle and camera
tests, plus two complete minified permission/camera journeys. Qualify physical camera
interoperability, cancellation under different driver behavior, and preview
orientation and process recreation on different devices. A
synthetic QR round-trip and a separate camera-frame test do not prove that full
journey. Retain no secret scan/import route or automatic transaction authority.

The bounded C request/reply codec and original-genesis fixtures are present.
The TLS/socket candidate is **BLOCKED — REQUIRES FURTHER SECURITY REVIEW**;
its [evidence is preserved](TLS_REVIEW.md), and normal host/Android builds
exclude it. The independent one-attempt C sync state and Android presentation
are implemented against isolated fixtures: C deadline/freshness and
offline balance state, late-token rejection and empty restart state are now
implemented, together with a bounded C owner pool and serialized JNI adapter.
JVM and emulator fixtures verify complete-only unverified reports, signed
pending deltas, deadlines, stale age and empty restart state. Managed owners
sample elapsed monotonic time inside serialization. Android presentation now
retains its original foreground owner and reads snapshots at UI delivery,
with queued-callback cancellation and empty replacement proven by local JVM
and main-queue fixtures. C now supplies an expiry delay to one owned main-queue
wakeup; delivery rechecks current state. The receive view renders explicit
unavailable/stale/unverified states, signed C amounts and this-address-only scope.
It rejects restored text state and starts with no amount. Nonempty display is
still local-fixture-only. Actual Activity recreation, foreground replacement
and deliberate emulator process relaunch now have public-fixture evidence.
The optional bounded C history exchange now shares the address/source/tip
lifetime and publishes only after its final tip. Its checked JNI projection
now has maximum-packet, exception and public-device fixtures. The history view
has unverified/empty/unavailable, restoration, detach and timer evidence. Both
views now share one foreground snapshot and clear together on rendering failure;
combined recreation, background/resume and process-relaunch fixtures pass.

Define the source trust and address-privacy model before enabling a real endpoint.
Verify the exact Zclassic chain/network identity, enforce deadlines and byte/count
limits, and report
offline/stale/unverified balances explicitly. Exercise malformed responses,
disconnects, retry bounds, reorgs and address/network mismatches against local
fixtures. No production node or mining runs on Worldstream.
The pinned transport-reference findings and original beta6 network differences
are recorded in [`READ_ONLY_SYNC.md`](READ_ONLY_SYNC.md).

Receiving QR encoding and device rendering are present. Physical camera QR
interoperability remains part of this gate.

## Implemented native transaction and change-state baseline

The bounded transparent v4 codec, exact draft construction, hash-matched previous
outputs, checked value/fee/destination assessment and immutable unsigned review
are implemented. Foreground review tests cover cancellation, expiry, recreation
and concealed text after rendering failure on host fixtures and Android emulators. The internal
candidate-context helper selects the pinned original Zclassic branch and checks
reviewed finality/expiry against explicit height/time inputs. P2PKH digests use
only the live review's transaction, input script and value. Raw-digest signing,
canonical signature-script verification and complete public signed-wire assembly
also have independent host and release-library fixture evidence. These primitives
grant no consent, authenticated chain state, unspentness or broadcast authority;
see [`TRANSACTIONS.md`](TRANSACTIONS.md) for their exact scope and remaining gates.

Change derivation, paired fresh wallet/index persistence, conservative index
reservation, consumed-address ownership and bounded authenticated suffix recovery
are implemented. The live-review wallet-input comparison checks re-derived
ownership under its authenticated-caller preconditions. Restoration still cannot
initialize index zero without historical discovery; an ambiguous or unsupported
tail still refuses recovery. Journal MACs do not establish recency against a
malicious filesystem rollback. These operations preserve their explicit per-use
custody requirements; see [`CHANGE_STORAGE.md`](CHANGE_STORAGE.md).

Native recovery/key regression coverage includes published vectors, independent
OpenSSL seed/BIP32/address comparisons, provider faults, exact scratch/context
cleanup, bounded differential fuzzing and mutation checks. Prepared HMAC state
reduces measured seed-derivation work while retaining the exact BIP39 profile.
These results support the current primitives; positive hardware custody still
requires the first acceptance gate above. Continue improvements from these
validated components while retaining the separate authorization gates for send.

## Remaining ordered scope

1. Transparent send: compose the existing draft/review, durable change ownership,
   pinned branch/sighash and signed-wire primitives with fresh authenticated
   chain context, per-use custody and explicit user consent. Recheck live review
   ownership and actual completion/delivery deadlines; qualify cancellation,
   process recovery and broadcast lifecycle before exposing send. No real funds
   in tests, and no server balance/history assertion as spending authority.
2. Shielded transactions: preserve original Zclassic wire/branch/proof semantics;
   qualify parameters, witnesses, anchors, value accounting, recovery and proof
   generation/verification before exposing a send action. No Rust toolchain.
3. Lightweight mobile validation: state precisely which proofs are checked and
   which trust assumptions remain. No consensus shortcuts or block exceptions.
4. Separate encrypted-messaging design: independent identity/key lifecycle,
   authenticated off-chain messages, bounded parsing/queues, replay protection,
   and no access to spending keys, signing or consensus authority.

No phase is considered complete merely because a build or narrow test passes.
