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
