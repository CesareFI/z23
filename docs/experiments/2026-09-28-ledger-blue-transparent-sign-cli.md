<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Ledger Blue transparent signing command

Local time: 2026-09-28T20:59:23-04:00. UTC: 2026-09-29T00:59:23Z.
Compiler: Clang 22.1.6. CPU: AMD Ryzen 7 PRO 8840U w/ Radeon 780M Graphics.

## Question

Can the existing mainnet preflight, Blue output review, previous-output
binding, signature verification, and authenticated transaction assembly form
one opt-in host command without trusting a host-selected input path?

## Finding

`zcl-blue-wallet-sign --sign-test` now takes an unsigned transaction, one
complete previous transaction per input, a local `zcl-rpc` executable, and
an absent output path. It checks the local node tip and UTXO status before
review, again before signing, and after assembly. The Blue must independently
bind every previous output to its external or internal key before the host
asks for physical signing approval. The host checks each returned path,
public-key hash, ZIP-243 digest, and ECDSA signature, then authenticates the
assembled signed wire against the reviewed unsigned-wire hash. It saves only
after an exact review-abort acknowledgement. No broadcast code is present.

Protocol 12 exports only the external public key. The host therefore marks
every other P2PKH input as a provisional internal-path candidate. This is
not proof of ownership. The simulated Blue refuses a previous output whose
hash matches neither device key. The two-key end-to-end simulator test uses
the same provisional classification and assembles a verified two-input
transaction. The CLI integration test runs valid local-node preflight with
no Blue attached, requires refusal, and confirms no output file appears.

The command holds one signed-wire buffer of at most 2 MiB, a 16-entry
signature array, and the existing bounded review job. The signed buffer and
signatures are erased before release. The output file is mode 0600, created
only if absent, and removed after reported failure if its inode still matches
the created file. An abrupt process death can leave an empty reserved output
file; this remains a cleanup limitation.

## Reproduction

```sh
cmake -S apps/zcl-ledger -B /tmp/z23-blue-standalone-release -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/z23-blue-standalone-release --parallel 4
ctest --test-dir /tmp/z23-blue-standalone-release --output-on-failure --parallel 1
cmake -S apps/zcl-ledger -B /tmp/z23-blue-standalone-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/z23-blue-standalone-debug --parallel 4
ASAN_OPTIONS=detect_leaks=0 ctest --test-dir /tmp/z23-blue-standalone-debug --output-on-failure --parallel 1
make check-core-seal check-cyclomatic-complexity
ZCL_WINDOWS_ACCEPTANCE_GUARD_SCRATCH=/tmp/z23-blue-sign-cli-wag make lint-fast
make check-markdown-links check-doc-inline-paths
```

All 59 Blue CTest cases passed in Release and sanitized Debug. Both full
builds use `-Wall -Wextra -Werror -pedantic`. The signing command links
Z23's C23 SHA-256 and RIPEMD-160 plus the pinned libsecp256k1 archive; it
does not link OpenSSL.
The core seal matched 554 files and 80 sections. Cyclomatic complexity
passed at the unchanged cap of 15. Fast lint passed 33 gates. Markdown link
and inline-path checks passed with 548 documents scanned and no new path
failures.

## Limit

The test driver has not signed a mainnet transaction on a physical Blue. The
current Wallet image remains blocked from physical installation after the
0.3.4 startup freeze. Simulated device behavior and local-node checks cannot
establish physical BOLOS behavior, independent chain synchronization, or
installed-image authenticity. Shielded payments, memos, multisig, and tokens
are outside this command's accepted wire format.
