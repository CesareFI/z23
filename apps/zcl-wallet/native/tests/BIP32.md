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
`2^31-1`, on both external0 and internal1 chains. It makes96 comparisons.
Build with `-DZCL_ORACLE=ON`; OpenSSL is a
host test dependency and is never linked into the Android app.

The public `zcl_change_from_entropy` API selects the
[BIP44 internal branch](https://github.com/bitcoin/bips/blob/master/bip-0044.mediawiki#change)
at m/44'/147'/0'/1/index (testnet coin1). The original receiving API retains
external0. Both wrappers share the existing private derivation and deterministic
secret cleanup. Neither exposes a key or reserves/persists an index. Change
ownership must later be bound to authenticated wallet state and durable index
recovery before any transaction is approved; the numeric assessment does not
grant that authority.

Deterministic fixtures check bounds, branch separation, blinding independence,
context/allocation failure on both branches, invalid children at each of the
five path levels without retry, and hash-provider failures across seed/HD/
HASH160/checksum construction. Interposed zeroization verifies cleared spans,
and the EC allocation wrapper requires cleared storage before each free. All
injected failures preserve output bytes and lengths. The bounded change fuzzer
always executes complete derivations as well as malformed argument cases.

The exact invalid-child result is returned to the caller; these APIs do not
silently alter a BIP32 path. Future address discovery may explicitly request
the next index with range checks. WIF, wallet.dat and xprv import are not exposed.
Public Base58 decoding of xprv strings occurs only for these published fixtures.
