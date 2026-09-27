# Security boundary

Only the existing Zclassic chain and ZCL are in scope. No new currency,
consensus modification, sharding, marketplace, Tor exit or mining behavior.
Worldstream is development-only. No task may start a production node, touch
canonical wallet/datadir files, spend real funds, or export existing secrets.

## Phase 1 design

* Money uses checked integer zatoshis, never binary floating point. Parsers
  reject ambiguous, oversized, overflow and out-of-network inputs.
* Address validation is Base58Check plus the exact network/type prefix and
  payload length. Zclassic shares some encodings with Zcash: an address alone
  cannot prove which chain a payer intended. Chain selection must be explicit.
* Secrets originate from the device CSPRNG. Android Keystore holds a wrapping
  AES-256-GCM key, requires device authentication and hardware security, and
  encrypts wallet secrets with authenticated format/network metadata. Missing
  or invalidated keys fail closed; they never silently replace a wallet.
* Wallet ciphertext belongs in private no-backup storage. Cloud/device backup,
  cleartext HTTP, screenshots and secret logging are disabled. Secret UI must
  not enter saved state, clipboard, intents, analytics or crash reporting.
* Portable C17 owns core logic under the explicit safety contract in
  `../AGENTS.md`. Buffers have explicit lengths and bounded ownership; private
  material receives explicit clearing. Kotlin/JNI only adapts Android platform
  services. Managed runtime and cryptographic provider copies require explicit
  review. Do not describe Keystore wrapping as hardware secp256k1 signing.
* Secret JNI output destinations have a managed owner before native execution.
  C returns a checked length and clears native scratch; managed finally clears
  failed/partial output and temporary prefix-copy buffers while preserving the
  original exception. Full-capacity success transfers the sole array to its
  caller. No native-created secret result may become unreachable on VM failure.
  This ownership rule does not erase every managed runtime/provider/UI copy.
* Read-only network code receives only public addresses and configuration.
  Endpoint responses are untrusted, bounded, tied to a network and displayed
  with their actual verification level. Unknown or stale balance is not zero.
* Authentication gates key operations. A QR scan never authorizes a payment.
  Send and shielded features remain unavailable until their acceptance gates
  pass; unsupported chain formats must fail closed.
* Untrusted request labels and messages reject Unicode 17 control/format
  characters and the line/paragraph separators U+2028/U+2029. Mandatory text
  breaks cannot introduce another apparent review field. Ordinary spaces and
  visible Unicode remain supported; this is not a complete spoof detector.
* Platform workers have at most two process owners, one worker and four queued
  inputs per owner. Closing clears queued inputs and lets an active operation
  finish its cleanup. Pool termination clears the session before returning its
  admission slot, without allocating another worker. Session cleanup may run on
  the closing thread only after no worker can access that state; it must clear
  owned data without blocking or calling UI/provider code. It releases retained
  callback references, and even a failing finalizer returns admission once.
  The session prepares its bound finalizer during empty construction, before
  any secret or worker admission, so requesting shutdown does not first need
  to allocate that callback. Active workers still retain cleanup ownership
  until termination; this does not make all framework shutdown allocation-free.
* Wallet Activity destruction first detaches its session and refuses further
  foreground work. It attempts prompt cancellation, worker closure, setup-timer
  removal and secret-view clearing even if another cleanup throws, then always
  attempts Android's base destruction. Owner cleanup preserves its first failure
  without allocating suppressed-exception storage. Partial construction is
  supported. An active worker still owns its secret until safe termination;
  destruction must not race it by clearing worker-owned entropy from the UI.
* Wallet failure handling detaches its session before attempting timer removal,
  worker closure and secret-view clearing. One cleanup error cannot skip the
  other owners or leave the recovery display active. Secret views are cleared
  before allocating the retry callback or failure UI. The first error still
  propagates even if later cleanup/rendering also fails; active worker entropy
  retains its existing safe termination ownership.
* Setup cancellation/restart detaches its old session and blocks further work
  before attempting prompt, timer, worker and secret-view cleanup independently.
  It propagates the first cleanup error and creates no replacement on failure.
  Secret views clear before replacement-session allocation; active entropy
  remains owned by the retiring worker until safe termination. A successful
  restart inspects storage again and requires a fresh explicit user action.
* Camera startup returns its single process admission if worker construction or
  startup fails before publishing a handler, including on fatal allocation
  errors. That failed lifetime cannot retry itself. Once an OS camera open is
  pending, cancellation retains admission until its real terminal callback;
  a timeout does not authorize a second owner.
  Its release callback is prepared before admission and reused by close();
  shutdown does not first allocate that callback while retaining camera
  resources. Handler internals may still allocate their queue messages.
* Scanner shutdown attempts camera, isolated-decoder and preview cleanup even
  if another owner throws. It preserves the first failure and retains failed
  owners for a later lifecycle cleanup attempt. Android's pause/destroy cleanup
  still runs; one component's exception cannot skip retirement of its peers.
* The decoder service erases its managed camera frame after native decoding,
  before returning text through Binder. It retains its single-input admission
  until the reply finishes and clears decoded text afterward. Task cleanup also
  erases cancelled/rejected frames. This does not erase all Binder/provider copies.
* Camera preview scratch pixels clear after upload or failure. Cleanup retires
  the owned image reference and clears scratch before calling Android's bitmap
  erase operation; it does not allocate a replacement buffer or recycle an image
  that rendering may still reference. Android/provider rendering copies cannot
  all be guaranteed erased. A rejected frame retires the earlier preview.
* Composite balance/history updates conceal both views before clearing or
  formatting. Both become visible only after the entire update succeeds. A
  failed clear still attempts its peer; a persistently refused text clear leaves
  both hidden. Primary and cleanup failures propagate without promoting stale
  values or an incomplete report. This does not qualify any network source.
* Recovery displays and keyboard previews conceal their text before clearing,
  transferring or rendering it. Only a complete update reveals the view again.
  Explicit cleanup erases owned character buffers before calling Android's
  visibility/text operations, so a stalled framework clear cannot prolong them.
  A refused Android text clear still wipes owned characters and leaves retained
  framework copies hidden. Input failures preserve the usable keyboard and
  original/cleanup errors. Concealment does not erase every framework or GPU copy.
* Backup-screen construction owns incoming words from entry. If layout or an
  action control fails after the words have been displayed, it clears the owned
  array and conceals/clears the view before propagating the original error.
  A failed framework clear remains hidden and is retained as a cleanup error.
* Create/restore setup shares one immutable elapsed-clock origin between its UI
  and worker, sampled after prompt approval and before secret work. C enforces
  the ten-minute bound at actual recovery display/retry/submission, worker use
  and persistence admission. Retries cannot restart that origin. A delayed
  Handler callback schedules cleanup; it does not grant authority after expiry.
  Already-admitted persistence must finish its bounded durability protocol.
  This does not guarantee instantaneous erasure while Android suspends a process
  or stalls its queues, or cancel a provider call already in progress.
* After encryption or authenticated decryption, the platform worker checks its
  foreground owner's closed state before admitting record creation or pending
  promotion. Closing during provider work therefore prevents new persistence;
  it still lets the provider return before the worker erases its owned plaintext.
  The atomic closed-state read is the admission point. Closure after that point
  permits the existing C durability protocol to finish without interruption.
* Unsigned-review text replacements remain concealed until complete. If expiry
  or cancellation text cannot be rendered, earlier transaction details stay
  hidden even when Android refuses clearing. Native review closure and display
  failure remain separate: a copied snapshot never keeps a review alive or
  supplies consent, authenticated chain state or signing authority.

## Evidence required before custody release

Device tests must cover authentication cancellation/timeout, lock removal,
biometric changes, Keystore invalidation, process death, corruption, truncated
writes, storage failure, backup exclusion, lifecycle secret clearing, recovery
and an independently reproduced receiving address. A passing host test is not
device acceptance. A passing provider signature is not Zclassic compatibility.

## Primary references

* [Zclassic reference source](https://github.com/ZclassicCommunity/zclassic)
* [Android Keystore](https://developer.android.com/privacy-and-security/keystore)
* [Android build tool compatibility](https://developer.android.com/build/releases/agp-8-13-0-release-notes)

Pin reference commits and test-vector provenance before accepting transaction
compatibility. Do not silently adopt modern Zcash network upgrades or SDKs.
