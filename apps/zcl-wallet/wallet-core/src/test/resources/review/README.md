# Public unsigned-review fixtures

These synthetic v4 transparent transactions contain no wallet, secret, real
funding or chain evidence. `native/tests/seed_assessment.c` emits them from
`assessment_fixture.c` into a new directory. Existing files refuse overwrite.
The Android test source set shares this JVM resource directory; application
APKs must contain none of these fixtures.

`draft` consumes previous0 output0 (10000 zatoshis, P2PKH hash11 repeated) and
previous1 output1 (1000, P2SH hash44 repeated). It pays9000 to P2PKH hash55 and
1500 to P2SH hash66, leaving500 fee. Lock and expiry are0; both sequences are
UINT32_MAX. No output is classified as change.

OpenSSL independently computed the displayed SHA256d draft ID:
`602c673db0503b48a400414347009ae968b1aaba9b10663bde508e7f8218dc46`.

SHA256 file identities:

```text
f1c68bbe02cd749ef8b0b974f0ce94b014177e90a5a40d4fac5de0ae6d4d9fe3  draft
1f33d5f213898defe284eb8f92fce8c8041b4d284357a0a79b840347914dc3be  previous0
046c83cfd9c24ecff698cd3e702006f1600fd87128e5f1b66460e0042926dc20  previous1
```

The codec created these fixture bytes; they qualify JNI transfer/lifetime and
presentation mapping, not independent original-node transaction acceptance.
