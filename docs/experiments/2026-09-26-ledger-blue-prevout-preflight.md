<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Transparent prevout binding before Ledger payment approval

Local time: 2026-09-26T23:51:05-04:00

UTC: 2026-09-27T03:51:05Z

## Question

Can the C23 host derive transparent input amounts, scripts, and a fee from
previous-transaction bytes bound to the spending transaction, instead of
accepting unverified host-provided amount and scriptCode values?

## Method

For each input of an unsigned, all-transparent v4 transaction, the new
preflight parses a supplied complete v4 previous transaction, computes its
SHA-256d txid, checks the 32 outpoint bytes and output index, and extracts a
P2PKH output. It rejects duplicate outpoints, nonempty input scripts,
unsupported output types, shielded components, more than 16 inputs, and a
negative fee. The hash-bound ZIP-243 function derives its scriptCode and
amount from that selected output. The limit is explicit and fails closed.

The double-hash byte order was checked against [ZIP 243 vector 3's published
txid](https://zips.z.cash/zip-0243#test-vector-3). The [Sapling protocol
specification](https://zips.z.cash/protocol/sapling.pdf) specifies SHA-256d
for v4 transaction identifiers. Unit tests also cover the synthetic
one-input 50,000,000-zatoshi prevout, two outputs totaling 49,000,000
zatoshi, and the resulting 1,000,000-zatoshi fee. A digest using the bound
script and amount matches the existing ZIP-243 implementation. A second
fixture binds two distinct prevouts and derives a 51,000,000-zatoshi fee.
Mutation and
truncation tests reject changed previous transactions, duplicate inputs,
invalid output type, out-of-range output index, negative fee, hidden
valueBalance, and truncated spending or previous transactions.

The Clang 22.1.6 Debug build uses AddressSanitizer and
UndefinedBehaviorSanitizer. GCC 16.1.1 builds the Release variant. CPU: AMD
Ryzen 7 PRO 8840U with Radeon 780M Graphics. Both local test suites pass
16/16. The cyclomatic complexity cap is 15 and passes.

A Clang libFuzzer target instruments the parser, input and output visitors,
script classification, and ZIP-243 code. The published 245-byte vector 3
transaction seeded a fixed 20,000-run session:

```sh
cmake -S apps/zcl-ledger -B build/zcl-ledger-fuzz \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang \
  -DZCL_LEDGER_FUZZ=ON
cmake --build build/zcl-ledger-fuzz --target fuzz-zcl-tx
mkdir -p /tmp/zcl-fuzz-corpus
tail -1 apps/zcl-ledger/tests/fixtures/zip243-transparent-vector3.hex \
  | xxd -r -p > /tmp/zcl-fuzz-corpus/zip243-vector3
build/zcl-ledger-fuzz/fuzz-zcl-tx /tmp/zcl-fuzz-corpus \
  -seed=2432026 -runs=20000 -max_len=512 -timeout=2
```

The observed run completed without a sanitizer finding. Its final libFuzzer
coverage counter was 8,754 and its feature counter was 9,080. These are
instrumentation counters, not percent coverage or a security proof.

## Security boundary

Hash-bound bytes alone do not prove a coin exists, is unspent, is mature, or
belongs to the selected Ledger key path. This host preflight does not verify
chain state, network upgrade branch ID, device display, touch approval, or
signature authorization. The Blue app has not been changed or installed by
this experiment. Payment signing remains disabled.
