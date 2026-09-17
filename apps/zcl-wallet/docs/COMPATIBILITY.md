# Zclassic compatibility record

Reference: [ZclassicCommunity/zclassic at 14a83d510ffd109d3fa09bf74ebf8c28854a263f](https://github.com/ZclassicCommunity/zclassic/tree/14a83d510ffd109d3fa09bf74ebf8c28854a263f).
Observed 2026-09-11. Source values must be tested, not inferred from Zcash SDKs.

| Format | Mainnet | Testnet/regtest |
| --- | --- | --- |
| P2PKH Base58Check prefix | `1cb8` | `1d25` |
| P2SH Base58Check prefix | `1cbd` | `1cba` |
| Transparent hash payload | 20 bytes | 20 bytes |
| WIF prefix | `80` | `ef` |
| WIF secret scalar | 32 bytes, optional `01` compressed marker | same |
| One ZCL | 100,000,000 zatoshis | same |
| Money range | 0–21,000,000 ZCL | same |

Sources: `src/chainparams.cpp`, `src/amount.h` and the public address/script
fixtures in `src/test/data/base58_keys_valid.json` at the pinned reference.
The address tests use only public addresses and output scripts. Testnet and
regtest share address encodings and therefore need independent chain identity.
Some encodings also match Zcash: the text cannot prove Zclassic intent.

Mainnet genesis hash:
`0007104ccda289427919efc39dc9e4d499804b7bebc22df55f8b834301260602`.

Mainnet upgrade heights in the reference are Overwinter/Sapling 476969,
Bubbles 585318, DiffAdj 585322 and Buttercup 707000. These are observations
for later acceptance; this app does not yet implement a header validator or
claim transaction-signing parity. Modern Zcash NU5/Orchard/ZIP-244 formats
must not be substituted for this chain.

## Complete v4 source layout reference

The internal source inspector uses pinned
[`src/primitives/transaction.h`](https://github.com/ZclassicCommunity/zclassic/blob/14a83d510ffd109d3fa09bf74ebf8c28854a263f/src/primitives/transaction.h)
and [`src/consensus/consensus.h`](https://github.com/ZclassicCommunity/zclassic/blob/14a83d510ffd109d3fa09bf74ebf8c28854a263f/src/consensus/consensus.h).
The post-Sapling transaction bound is 102,000 bytes. Serialized spend, output
and Groth JoinSplit descriptions occupy 384, 948 and 1698 bytes respectively;
nonempty JoinSplits append 96 bytes, and any Sapling spend/output appends a
64-byte binding signature. These observations specify wire parsing only.

| Pinned file | SHA256 |
| --- | --- |
| `src/primitives/transaction.h` | `5296163b58f516c3a0cc2b6f46d55bf2195d371b1ab81a312fb97de48a9c7ece` |
| `src/consensus/consensus.h` | `0948bd9543235003cd2a5f03312bb9bdbd0db9f3deca2b413d5cdb51def7b82d` |
| `src/test/data/sighash.json` | `6e10f3e5649c876a8968a3ba13885aeb8dcee8040fd89e7e39651b041d07f30c` |

From the wallet directory, run
`bash tools/project-original-sources.sh /path/to/pinned-sighash.json /new/output`
and compare `/new/output/source_vectors.h` with `native/tests/source_vectors.h`.
The script checks the input hash, extracts untouched rows 153/203/208/296,
checks complete layout and computes identities with OpenSSL. It executes no
reference node. Row153 has an invalid consensus expiry; tests preserve its raw
field and zero expiry only in a separate transparent output projection.
No vector is claimed to be valid, included or spendable.
