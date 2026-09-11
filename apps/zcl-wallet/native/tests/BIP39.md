# Public BIP39 fixtures

`bip39_vectors.inc` extracts the 24 English entropy, mnemonic and seed triples
from trezor/python-mnemonic `vectors.json` at commit
`b57a5ad77a981e743f4167ab2f7927a55c1e82a8`. Every seed uses passphrase `TREZOR`.
The source JSON SHA-256 is
`fa3b937b7cff9c9b8ecd3aa011faeb8d6dd67993174b72326e83f4de8fdb30f8`.
The source MIT license is retained at repository `vendor/android-bip39/LICENSE`.
These are public test secrets and must never control funds.

Each generated record stores explicit entropy and mnemonic lengths. The test
compares phrase encoding, entropy decoding and the 64-byte seed independently
against the published values. Additional checks exercise all five standard
entropy sizes, capacities/canaries, malformed words, checksum rejection and
unsupported passphrase normalization. This is recovery-code evidence only.
