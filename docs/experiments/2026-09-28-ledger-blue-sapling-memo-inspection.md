<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Blue Sapling memo inspection

Local time: 2026-09-28T05:08:50-04:00

UTC: 2026-09-28T09:08:50Z

The C23 memo inspector classifies exactly 512 decrypted Sapling memo bytes
under [ZIP 302](https://zips.z.cash/zip-0302). It distinguishes UTF-8 text,
the exact no-memo marker, opaque data, reserved future encodings, and invalid
UTF-8. Text length excludes trailing zero padding. Embedded zero bytes are
reported explicitly so a future display cannot silently truncate a memo.
SHA-256 covers the complete 512-byte field, including padding or opaque
bytes. The result is provisional: the Blue must verify epk, note commitment,
recipient policy, and transaction binding before displaying any memo as an
approved payment fact or signing.

The existing consensus-accepted deterministic simnet transaction decrypts
successfully but its memo starts with `0xf6` and is not followed by 511 zero
bytes. The inspector classifies it as a reserved future encoding, never as
“no memo.” The test checks this on the authenticated ciphertext recovered from
the six-pass captured output.

The host test covers UTF-8 boundaries, overlong sequences, surrogate and
out-of-range code points, embedded zero bytes, exact no-memo matching,
reserved prefixes, opaque prefixes, and full-field hash sensitivity.
Clang 22.1.6 Release and sanitized Debug passed the focused tests on an AMD
Ryzen 7 PRO 8840U. The same C23 implementation passed in the Cortex-M3 and
Cortex-M0 QEMU cases. The memo case used at most 1,232 bytes of modeled M3
stack and 1,272 bytes of modeled M0 stack; the case harness requires 512
bytes free in its 2,048-byte reserved test stack. These counts exclude BOLOS,
USB, and screen frames and do not establish an installed app memory budget.

Reproduction from the Blue source checkout:

```sh
cmake -S apps/zcl-ledger -B /tmp/z23-blue-release -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/z23-blue-release -j2
ctest --test-dir /tmp/z23-blue-release -R '^blue-sapling-(memo|ock)$|^blue-(m3|m0-sapling)-qemu$' --output-on-failure
```
