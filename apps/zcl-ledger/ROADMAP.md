<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# ZCL Wallet for Ledger Blue

The target is **one C23 ZCL Wallet app** on the dedicated Blue, controlled by
Z23's C23 host. It should handle address confirmation and supported ZCL
payments from one device interface. Probe, Fixture, Review, and Sign Test are
separate development images used to test USB, display, parsing, and key
access; they are not four apps required for a payment. A second production
app would be justified only by a measured Blue firmware or memory limit.

The host selects coins and notes, obtains consensus data from the Z23 node,
constructs transactions and Sapling proofs, and checks completed signatures.
The Blue derives authorized keys, independently checks the transaction facts
it can verify, displays the recipient, amount, network, fee, and signing
account, and signs only after the owner approves those exact facts. An opaque
digest with host-supplied display text is insufficient for payment approval.

## Agent-facing control

Z23's current safe automation sequence is device discovery, exact app identity,
offline transaction parsing, offline app-controller simulation, and an optional
read-only Blue comparison after the owner opens the Review app. JSON keeps
`app_simulated` and `blue_parsed` separate. Neither status grants payment
authority; `signing_ready` remains false. A timeout or mismatched reply ends
that attempt and requires a fresh device-state check before another command.
The agent must not infer permission to install, delete, enroll a certificate,
or sign from a successful simulation or structural review. Future payment
signing requires a purpose-built approval screen and the owner's confirmation
on the Blue for the exact transaction facts.

| Capability | Proven today | Required before payment use |
| --- | --- | --- |
| USB and app management | Authenticated CA install and app catalog on BOLOS 2.1.1 | Repeatable install, open, exit, and recovery checks for the final app |
| Screen accessibility | C23 offline screenshots, word-wrapped 22-pixel text, and light/dark controls in the uninstalled Review 0.4.4 candidate; uninstalled Wallet 0.2.15 uses 22-pixel output and input-path labels and has 320×480 internal-address, fee, totals, and confirmation previews; Wallet 0.2.1 physical review froze | Physical font, color, tap, page, EXIT, fee, and USB checks before inclusion in a signing app |
| Transparent addresses | Public-key fixture and fixed-message seed-derived signing test; Wallet 0.2.1 opened to a steady receive screen with working EXIT, and its 35-character address matched the host result; the app was later deleted after a review freeze | Selectable path and account, repeated receive and interruption checks |
| Transparent payments | The host displays P2PKH/P2SH outputs and preflights up to 16 hash-bound v1-v4 P2PKH inputs; uninstalled Wallet 0.2.15 binds those wires, requires input hashes to match its two fixed public keys, rejects unknown mainnet branch IDs, derives a fee, shows verified input paths and device-derived own/other output totals, and returns one input-specific ZIP-243 digest per bound input for independent host comparison. The host derives the branch from a local mainnet node tip and checks each input's value and script hash against that node's UTXO view; independent peer synchronization and complete ownership policy remain unchecked. Read-only CONFIRM cannot arm the unrouted signing path. Physical Wallet 0.2.1 review froze before an output was confirmed; exact P2PKH matches to device-derived external 0/0 and internal 1/0 addresses are distinguished without claiming change | Diagnose and pass BOLOS USB/touch, fee, digest, and recovery tests; verify independent chain state, derive change and account on device, and require a separate explicit payment approval before signatures |
| Sapling payments | Host and Review compute ZIP-243 digests; no Sapling keys or signing in Review | ZCL branch selection, key derivation, note and output binding, exact on-device review, spend authorization, and end-to-end test transactions |
| Sapling memos | Z23 wallet stores 512-byte decrypted note memos; Review sees only encrypted transaction bytes | Classify absent, UTF-8, and opaque memo forms; show text or an explicit binary warning; bind any displayed outgoing memo to a device-verifiable transaction commitment before signing |
| Transparent multisig | Host recognizes P2SH scripts but cannot infer a threshold | Redeem-script and cosigner validation, device confirmation, and independently verified signatures |
| ZSLP | Host detects an output-zero marker | Token parser, input lineage and supply validation, token-aware display, and test transactions |
| Shielded multisig | No threshold signing | Verify ZCL compatibility and implement a multiple-party Sapling signing protocol with nonce safety and test vectors |
| Legacy Sprout | Structural JoinSplit count only | Determine whether current ZCL consensus permits the intended spend and whether the Blue can safely review and authorize it |

The release sequence is: reliable catalog and review; transparent receive and
payment signing on synthetic fixtures; test-network transactions; Sapling
spends and receives; token-aware and multisig flows. Every signing path must
reject a transaction type it cannot fully review. The Blue has 6 KiB of app
SRAM, including its reserved stack, so larger transactions require bounded
streaming and measured stack use. The uninstalled, read-only Wallet 0.2.15 candidate
uses one BLAKE2b context and a SHA-256 commitment across three identical
uploads, then reuses that SHA-256 context to verify previous wires. Its
5,472-byte `.bss` includes the 2 KiB stack reservation and leaves 672
bytes after the section. The read-only reviewer can build without OpenSSL or
PNG, but the installer and certificate tools still require OpenSSL. A signing
design still needs independently verified chain state, branch selection,
change, and payment approval.
The [Wallet 0.2.15 experiment](../../docs/experiments/2026-09-27-ledger-blue-local-sha-and-readonly-approval.md)
records the host tests, image hash, and SDK patch gate.
Device tests start on the dedicated test
Blue and use test transactions before any real ZCL.

Sapling shielded multisig is a separate research gate. [ZIP 312](https://zips.z.cash/zip-0312)
specifies a FROST variant designed to verify as ordinary RedJubjub Sapling
spend authorization signatures. Its coordinator generates the proof and can
learn private transaction details. Compatibility with ZCL's deployed
consensus, BOLOS Jubjub support, nonce storage, and the Ledger Blue's RAM and
timing remains unestablished. First create reproducible ZIP 312 vectors and
verify their signatures against ZCL consensus in C23, then measure a bounded
two-round participant prototype
on the Blue. No threshold or Sapling payment signer is ready today.

[ZIP 302](https://zips.z.cash/zip-0302) distinguishes padded UTF-8 text,
the no-memo marker, reserved encodings, and opaque binary memos within the
512-byte encrypted Sapling memo field. A receiving wallet can classify a
memo only after note decryption. An outgoing signing screen must bind the
displayed memo or explicit opaque-data digest to the transaction before the
device releases a signature; host-supplied display text alone is insufficient.
