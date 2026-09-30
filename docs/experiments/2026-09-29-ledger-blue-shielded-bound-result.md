<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->
# Bound result from Blue shielded read-only review

## Question

Does the host retain the full-wire commitment that it checked against the
Blue's six-pass replay, or does it recompute a potentially changed wire only
when presenting the CLI result?

## Finding

The read-only client previously checked the device's full-wire SHA-256 and
discarded it. The CLI then hashed its caller-owned wire again for display.
The read-only client now returns one receipt containing the checked 32-byte
commitment, ZIP-243 digest, parsed facts, branch ID, and wire length. The CLI
prints the receipt's commitment, branch, and length. A compatibility entry
point retains the existing read-only API.

The receipt entry point rejects output storage overlapping the wire before
writing or sending an APDU. A single receipt keeps its checked fields
together. It zeros the entire receipt after a failed replay, including when
an erase callback poisons it. The host test matches the returned commitment
against both the simulated Blue reply and an independent SHA-256 of the
fixture. Changing a proof byte leaves ZIP-243 unchanged and changes the
returned full-wire
commitment. Changing the caller wire after return also breaks that binding.

The commitment describes one completed read-only review. It does not freeze
the caller's wire after return, prove shielded recipient or fee facts, or
authorize signing. A later signing flow must retain an immutable wire and
recheck its full-wire commitment through final approval and assembly.

The single-receipt revision also tests a callback poisoning the result just
before successful completion, an alternate known branch, and receipt/wire
aliases. Success clears the receipt, then copies each parsed fact field and
the other checked values from the verified snapshot without copying struct
padding. Non-overlap failure leaves every byte zero; overlap rejection preserves
caller storage. This revision adds no device signing route.

## Reproduction and limits

At 2026-09-29T06:41:35-04:00 (2026-09-29T10:41:35+00:00), the host was an AMD
Ryzen 7 PRO 8840U with Clang 22.1.6.

```sh
cmake --build /tmp/z23-blue-standalone-release -j4
ctest --test-dir /tmp/z23-blue-standalone-release -j1 --output-on-failure
cmake --build /tmp/z23-blue-standalone-debug -j4
ASAN_OPTIONS=detect_leaks=0 LSAN_OPTIONS=detect_leaks=0 \
  ctest --test-dir /tmp/z23-blue-standalone-debug -j1 --output-on-failure
rg -v '^#' apps/zcl-ledger/tests/fixtures/simnet-sapling-spend.hex \
  | xxd -r -p >/tmp/z23-blue-sapling-review-fixture.bin
sha256sum /tmp/z23-blue-sapling-review-fixture.bin
/tmp/z23-blue-standalone-release/zcl-blue-shielded-review --simulate \
  /tmp/z23-blue-sapling-review-fixture.bin 0x76b809bb
```

Release passed 62/62 in 29.62 seconds and sanitized Debug passed 62/62 in
33.77 seconds. The 1,425-byte fixture's SHA-256 and the simulated CLI's
printed full-wire commitment both equal
`683cec314305410a0d90146856ea503dffc1c74a8e01d6fa89eff79403cd2216`.
The changed source is host-only; no physical Blue interaction occurred.
Address and undefined-behavior instrumentation stayed enabled in Debug;
leak detection was disabled because its startup under process tracing is
blocked in this environment.

## Single-receipt revision

At 2026-09-29T06:56:41-04:00 (2026-09-29T10:56:41+00:00), the same Clang
22.1.6 host rebuilt the client with one receipt output. Release passed
62/62 in 30.95 seconds and sanitized Debug passed 62/62 in 35.09 seconds.
The simulated CLI printed branch `0x76B809BB`, wire length 1,425 bytes,
and full-wire SHA-256
`683cec314305410a0d90146856ea503dffc1c74a8e01d6fa89eff79403cd2216`.
The receipt and legacy read-only entry points both remain independent of
the Wallet signing APDU and device keys.
On this Clang C23 host, `sizeof(blue_shielded_review_receipt)` is 120 bytes
and `sizeof(zcl_tx_review)` is 48 bytes. The receipt stores its branch and
wire length with the hashes and facts; these host sizes are not Blue SRAM
measurements.
