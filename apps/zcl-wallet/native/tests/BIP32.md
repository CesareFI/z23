# Public BIP32 and receiving-address fixtures

`bip32_vectors.inc` extracts the 17 paths in BIP32 vectors 1..4 at bitcoin/bips
commit `86d96b63b063c40eafd5db65fff5aa118e6def59`. The source `bip-0032.mediawiki`
SHA-256 is `e5e00a8289db2f681052cf24a745320afc225e66b25d1e489a7c884d2fc7f11f`.
These published test secrets must never control funds. The test compares the
private scalar, chain code and compressed public key, including hardened and
normal derivation and leading-zero preservation.

`test_receive_oracle.c` independently implements the entire derivation using
host OpenSSL: empty-passphrase PBKDF2, HMAC, BIGNUM child addition, EC public
keys, HASH160, checksum and BIGNUM Base58. It shares no mnemonic/derivation/
hash/address implementation with the app. Inputs are six published English
BIP39 fixtures, both Zclassic networks and four address indices, including
`2^31-1`. It makes 48 comparisons. Build with `-DZCL_ORACLE=ON`; OpenSSL is a
host test dependency and is never linked into the Android app.

The exact invalid-child result is returned to the caller; these APIs do not
silently alter a BIP32 path. Future address discovery may explicitly request
the next index with range checks. WIF, wallet.dat and xprv import are not exposed.
Public Base58 decoding of xprv strings occurs only for these published fixtures.
