# Recovery and custody design

C recovery and receiving-key derivation are implemented and under checkpoint
validation. Android storage and lifecycle acceptance below are still pending.

The first recovery profile uses BIP39 English and the BIP44 transparent path
`m/44'/147'/0'/0/index` on Zclassic mainnet. Testnet uses coin type 1. Creation
uses 128 bits from the device operating system CSPRNG and a 12-word recovery
phrase. Existing 256-bit/24-word wallets remain recoverable; their keys and
records are unchanged. Restore validates all standard 12/15/18/21/24-word
checksums. There is
no brainwallet, custom wordlist or new key derivation scheme.

This is an explicit app recovery profile, not a promise that arbitrary legacy
Zclassic wallet.dat files, WIF keys, Electrum phrases or other HD paths restore
through it. The UI must name the supported format before accepting a phrase.
Initial UI creation/restore uses the empty BIP39 passphrase and must say so.
Future nonempty-passphrase support requires explicit UI and recovery tests;
unsupported normalization must fail instead of deriving a different wallet.

The supported transparent account is account 0. External receiving keys use
`m/44'/147'/0'/0/index`; internal change uses `m/44'/147'/0'/1/index`. Both use
coin type 1 on testnet. The receiving UI currently exposes external index 0;
the native API supports external indices below 2^31. Reserved internal change
indices are bounded to 0..65534 by the authenticated change journal. Recovery
must discover used receive/change outputs and establish a safe change frontier
before spending; restoring entropy alone neither discovers balances nor grants
permission to reset that journal. Other accounts, shielded spending keys and
shielded balance recovery are not implemented by this profile. A complete
network-backed send/restore/spend journey and physical-device custody remain
release requirements, not claims established by the mnemonic round-trip tests.

## Ownership and interfaces

Core APIs receive byte spans with lengths and caller-owned output capacities.
They retain no secret pointers or global wallet state. Mnemonic, entropy, seed,
private key, HMAC state and intermediate derivation buffers are secrets. Every
secret-producing call has one cleanup path that clears scratch storage on both
success and failure. The caller must clear its input/output secret buffers when
finished. An unsuccessful call leaves caller output unchanged.

Key derivation uses SHA-512/HMAC/PBKDF2 and libsecp256k1 primitives. No new curve,
signature algorithm or arithmetic implementation is introduced. A preallocated,
per-operation secp256k1 context has a checked storage bound and independent RNG
blinding. Its transient allocation is checked nonzero and capped at 1024 bytes;
the owner destroys, clears and frees it through one cleanup path. No live
private-key native handle crosses JNI. Deterministic failure tests use published
fixtures and test-only injected failure points, never production seeds.

Read-only networking receives public address records only. It cannot call secret
storage or a signer. Signing, when implemented, requires a separate authenticated
operation and validates the complete transaction before authorization.

## Android storage and lifecycle

The Android adapter generates a nonexportable AES-256-GCM wrapping key in Android
Keystore, requires hardware security and device authentication, and stores only
authenticated ciphertext in private no-backup storage. Public metadata is bound
as authenticated additional data. Encryption IVs come from the provider; a caller
cannot select or reuse them. Reads enforce exact format and size bounds before
decryption. A missing/invalidated wrapping key or corrupt record is an explicit
recovery state, never permission to create replacement keys silently.

A receiving address read from disk is not trusted before GCM authentication
and independent re-derivation from the decrypted entropy. Public-looking
metadata is still attacker-controlled storage input. The app must not display
an unauthenticated stored address for receiving funds.

Creation must confirm recovery backup before considering setup complete. A
temporary record must not overwrite an existing wallet. Storage requires an
atomic write protocol and failure tests, including process death. Existing user
wallets are outside development/test scope.

The adapter clears owned byte/character arrays in finally blocks and clears
secret views when backgrounded. It prevents screenshots, view state persistence,
autofill, clipboard export and suggestions on secret fields. Managed UI, input
methods and provider internals can make copies the app cannot reliably erase;
this limitation must remain explicit. Hardware wrapping is not hardware
secp256k1 signing. A compromised unlocked process can still expose plaintext.

## Acceptance before exposing creation or restore

* Pinned BIP39 entropy/phrase/seed vectors, all entropy sizes, checksum failures,
  bounded malformed input and scratch clearing.
* BIP32 hardened/normal and leading-zero vectors; independent Zclassic receiving
  address reproduction; invalid-child behavior and index boundaries.
* Failure injection for randomness, crypto operations and storage; no partial
  output, fallback entropy or silent key replacement.
* Native sanitizer/static analysis and explicit C hazard review; JNI exception,
  length and lifecycle checks; actual ARM64/x86-64 Android builds.
* Device evidence for authentication, hardware Keystore, cancellation,
  invalidation, backgrounding, process death, interrupted writes and recovery.

Pinned standards: [BIP39](https://github.com/bitcoin/bips/blob/86d96b63b063c40eafd5db65fff5aa118e6def59/bip-0039.mediawiki),
[BIP32](https://github.com/bitcoin/bips/blob/86d96b63b063c40eafd5db65fff5aa118e6def59/bip-0032.mediawiki),
and [SLIP-0044](https://github.com/satoshilabs/slips/blob/master/slip-0044.md).
The development evidence must distinguish completed tests from these planned
acceptance conditions.
