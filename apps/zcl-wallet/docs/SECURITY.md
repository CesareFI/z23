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
* Read-only network code receives only public addresses and configuration.
  Endpoint responses are untrusted, bounded, tied to a network and displayed
  with their actual verification level. Unknown or stale balance is not zero.
* Authentication gates key operations. A QR scan never authorizes a payment.
  Send and shielded features remain unavailable until their acceptance gates
  pass; unsupported chain formats must fail closed.
* Platform workers have at most two process owners, one worker and four queued
  inputs per owner. Closing clears queued inputs and lets an active operation
  finish its cleanup. Pool termination clears the session before returning its
  admission slot, without allocating another worker. Session cleanup may run on
  the closing thread only after no worker can access that state; it must clear
  owned data without blocking or calling UI/provider code. It releases retained
  callback references, and even a failing finalizer returns admission once.
* Camera startup returns its single process admission if worker construction or
  startup fails before publishing a handler, including on fatal allocation
  errors. That failed lifetime cannot retry itself. Once an OS camera open is
  pending, cancellation retains admission until its real terminal callback;
  a timeout does not authorize a second owner.
* Camera preview scratch pixels clear after upload or failure. Cleanup retires
  the owned image reference and clears scratch before calling Android's bitmap
  erase operation; it does not allocate a replacement buffer or recycle an image
  that rendering may still reference. Android/provider rendering copies cannot
  all be guaranteed erased. A rejected frame retires the earlier preview.

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
