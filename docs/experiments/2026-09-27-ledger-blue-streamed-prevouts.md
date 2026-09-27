<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Bounded previous-transaction verification for the Blue

Date: 2026-09-27. Transparent signing needs the device to obtain each input's
amount and P2PKH script from the exact previous transaction named by the
spending outpoint. The current Blue app only reviews the spending transaction.
Its host preflight checks supplied previous wires, but that host result must
not become a device signing fact.

`zcl_tx_previous_stream` is an offline C23 module for the device integration.
It parses exact v1/v2/Overwinter-v3/Sapling-v4 previous wires in chunks,
uses 168 bytes of host parser state and has a 192-byte compile-time cap. It
enforces canonical CompactSize, script and vector bounds, money range, version groups,
version-specific tails, and exact end of wire. The selected output must be
P2PKH. It SHA-256d hashes the same bytes through caller-supplied streaming
callbacks and returns the selected script and amount only if the digest
matches the expected transaction ID. Failed selection leaves the caller's
output untouched. The parser holds no previous transaction in RAM.

Clang 22.1.6 Debug with address and undefined-behavior sanitizers and GCC
16.1.1 Release each passed 24/24 host tests. The dedicated test builds v1-v4
previous wires, including legacy JoinSplit and v4 Sapling plus JoinSplit
tails. It compares the streaming result with the full-wire selector using
1-, 7-, and 128-byte chunks, and rejects wrong IDs, wrong output indexes,
and a changed selected script. Differential libFuzzer compared both parsers
for 100,000 mutations from four valid v1-v4 seeds under Clang sanitizers;
there was no mismatch or sanitizer failure. Its final report was 648 counters
and 1,935 features. The repository complexity ratchet passed at cap 15 over
60,466 functions in 4,413 files. CPU: AMD Ryzen 7 PRO 8840U with Radeon 780M
Graphics. An independent freestanding Clang 22 ARM Cortex-M3 C23 object
compiled with `-Wall -Wextra -Werror -pedantic`: `.text` 2,042 bytes,
`.data` and `.bss` zero, and the largest named stack frame 88 bytes. This
does not include the external SHA-256 context or BOLOS frames. These results
establish host parser behavior and ARM compilation only; there is no linked
app image, BOLOS execution, chain proof, UTXO check, or signing permission.

The next integration must bind the selected input outpoint observed in the
device's spending-transaction stream to this parser's expected ID and output
index, then derive fee from every bound input. Only a chain-verified unspent
view, device-derived account/change, explicit fee and all-output approval,
and an exact digest can precede a payment signature.

To repeat the host differential run, create the four short raw-wire seeds
using the commands in
[previous-output parser sanitizer fuzzing](2026-09-27-ledger-blue-prevout-fuzz.md),
then build and run:

```sh
cmake -S apps/zcl-ledger -B /tmp/zcl-prevstream-fuzz \
  -DCMAKE_C_COMPILER=clang -DCMAKE_BUILD_TYPE=Debug -DZCL_LEDGER_FUZZ=ON
cmake --build /tmp/zcl-prevstream-fuzz --target fuzz-zcl-previous-stream -j4
/tmp/zcl-prevstream-fuzz/fuzz-zcl-previous-stream \
  -seed=20260927 -max_len=4096 -runs=100000 /tmp/zcl-prevout-corpus
```
