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
| Screen accessibility | C23 offline screenshots, word-wrapped 22-pixel text, and light/dark controls in the uninstalled Review 0.4.4 candidate; the uninstalled Wallet 0.2.0 candidate links large address and amount pages with CONTINUE and EXIT, previewed at 320 × 480 | Physical font, color, tap, page, EXIT, and USB checks before inclusion in a signing app |
| Transparent addresses | Public-key fixture and fixed-message seed-derived signing test; fixed-path receive candidate passes offline host and ARM builds but has not run on the Blue | Physical address and EXIT checks, host/device address comparison, selectable path and account |
| Transparent payments | Host displays P2PKH/P2SH output facts and computes ZIP-243 input digests against a published vector; C23 host preflight binds up to 16 P2PKH inputs to supplied v4 previous-transaction bytes by txid and derives a fee; the linked Wallet 0.2.0 candidate parses unsigned v4 chunks, stops at each exact output boundary, shows the full address and amount, binds acknowledgement to a touchscreen callback, and validates identical three-pass replay; its exact image is reproducible and host-tested but uninstalled | Chain-verified unspent prevouts, device-derived change and account, on-device fee and all-output confirmation, physical output-page test, BOLOS USB/touch validation, and input-specific approval and signatures |
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
streaming and measured stack use. The linked, read-only Wallet 0.2.0 candidate
uses one BLAKE2b context and a SHA-256 commitment across three identical
uploads. Its 4,236-byte `.bss` includes the 2 KiB stack reservation and
leaves 1,908 bytes after the section. A signing design still needs trusted
prevouts, fee, change, and final approval.
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
