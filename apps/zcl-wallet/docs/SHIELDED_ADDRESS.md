# Shielded public address envelopes

`zcl_shielded_address_parse` is an offline, allocation-free C reader for public
Sprout and Sapling address **envelopes**. It checks encoding, checksum, exact
payload size and the explicitly selected mainnet/testnet prefix. It does not
validate curve points or diversifiers, prove ownership/spendability, derive a
key, authorize a payment, or establish chain identity. These encodings are
shared with other chains.

This is a foundation for future shielded wallet work. The existing transparent
payment, QR, JNI and UI APIs remain unchanged. There is no shielded send/receive
binding, proof generation, transport activation or custody-policy change.

## Compatibility scope

The reference is original Zclassic commit
`14a83d510ffd109d3fa09bf74ebf8c28854a263f`:

- `src/chainparams.cpp`: Sprout prefixes `169a` / `16b6`; Sapling HRPs `zs` /
  `ztestsapling`. Regtest's `zregtestsapling` is not a testnet alias.
- `src/zcash/Address.hpp`: Sprout `a_pk || pk_enc`, 64 serialized bytes;
  Sapling `d || pk_d`, 43 serialized bytes.
- `src/key_io.cpp`, `DecodePaymentAddress`: exact decoded length and network
  checks; Sapling conversion uses `ConvertBits<5,8,false>`.
- `src/bech32.cpp`: original Bech32 checksum (constant 1), uniform upper/lower
  case, mixed-case refusal. This is not Bech32m.

The C reader reuses the existing bounded Base58Check implementation for Sprout.
Its fixed-size Sapling reader implements the reference's Bech32 polynomial.
It accepts exactly 69 data symbols plus six checksum symbols and requires the
unused final data bit to be zero. It derives text bounds from fixed HRPs:
78 bytes on mainnet and 88 on testnet. Sprout text is 95 bytes. Whitespace is
rejected; the original generic Base58 decoder's surrounding-space tolerance is
intentionally not a wallet address-input rule.

The public API takes stable, nonoverlapping caller-owned spans, retains no
pointers and publishes the complete result only after verification. All
refusals leave the output unchanged. Sapling's unused output tail is zero.
No input causes allocation, recursion, logging, network access or secret use.

## Offline fixtures and limits

`native/tests/shielded_address_vectors.h` contains 21 synthetic public fixtures
projected using the pinned reference's unchanged Base58Check/Bech32 encoders
and `ConvertBits`, independent of the candidate reader. For each of all-zero,
all-`ff`, and increasing-byte payloads it includes:

- Sprout mainnet/testnet and Sapling mainnet/testnet successes;
- a regtest Sapling address rejected in testnet mode;
- mainnet/testnet addresses with valid checksums but a nonzero padding bit.

The reference decoder and bit converter separately checked the projected bytes
and padding refusals. The synthetic payloads do not claim usable spending keys
or valid Sapling points/diversifiers. Unit tests compare every decoded byte,
check both network selections and case handling, alter every character, and
exercise lengths, hostile bytes, output canaries and unchanged failure outputs.
`fuzz_shielded_address` also checks failure atomicity, mutually exclusive network
acceptance and Sprout re-encoding under the existing sanitized fuzz profile.

After the canonical C safety build, run the focused test from the app directory:

```sh
ctest --test-dir native/build/safety-active -R '^wallet_shielded_address$' --output-on-failure
```

This evidence covers envelope decoding only. Full shielded wallet, custody,
device lifecycle, proof validity and consensus equivalence remain separate
acceptance requirements.
