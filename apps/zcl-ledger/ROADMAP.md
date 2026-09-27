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

| Capability | Proven today | Required before payment use |
| --- | --- | --- |
| USB and app management | Authenticated CA install and app catalog on BOLOS 2.1.1 | Repeatable install, open, exit, and recovery checks for the final app |
| Transparent addresses | Public-key fixture and fixed-message seed-derived signing test | Device-confirmed receive address for a selected ZCL path |
| Transparent payments | Host displays P2PKH/P2SH output facts; device Review parses transaction structure | Streaming transaction review, trusted prevout amounts, complete output display, fee calculation, and input-specific signatures |
| Sapling payments | Host and Review compute ZIP-243 digests; no Sapling keys or signing in Review | ZCL branch selection, key derivation, note and output binding, exact on-device review, spend authorization, and end-to-end test transactions |
| Transparent multisig | Host recognizes P2SH scripts but cannot infer a threshold | Redeem-script and cosigner validation, device confirmation, and independently verified signatures |
| ZSLP | Host detects an output-zero marker | Token parser, input lineage and supply validation, token-aware display, and test transactions |
| Shielded multisig | No threshold signing | Verify ZCL compatibility and implement a multiple-party Sapling signing protocol with nonce safety and test vectors |
| Legacy Sprout | Structural JoinSplit count only | Determine whether current ZCL consensus permits the intended spend and whether the Blue can safely review and authorize it |

The release sequence is: reliable catalog and review; transparent receive and
payment signing on synthetic fixtures; test-network transactions; Sapling
spends and receives; token-aware and multisig flows. Every signing path must
reject a transaction type it cannot fully review. The Blue has 6 KiB of app
SRAM, including its reserved stack, so larger transactions require bounded
streaming and measured stack use. Device tests start on the dedicated test
Blue and use test transactions before any real ZCL.

Sapling shielded multisig is a separate research gate. [ZIP 312](https://zips.z.cash/zip-0312)
specifies a FROST variant for Zcash Sapling spend authorization; its
compatibility with ZCL's deployed consensus and Ledger Blue resources has not
been established. No threshold or Sapling payment signer is ready today.
