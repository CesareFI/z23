<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->
# Ledger Blue shielded review wire snapshot

Date: 2026-09-28T19:21:19-04:00 / 2026-09-28T23:21:19Z.

## Question

Can the shielded client report success for a caller transaction that changed
after the host computed the expected review and digest?

## Experiment and result

The simulated USB callback changes the caller's wire after the Blue returns
its final, valid digest. Before the fix, the client returned success with
facts and digest for the former wire. The Release regression failed at its
expected rejection assertion. The final test changes the wire after each of
the 50 exchanges in the synthetic one-spend/one-output Sapling replay. Each
case must reject, clear both outputs, request device erasure, and leave no
active or completed simulated review.

A second regression changes both output buffers inside the best-effort erase
callback after a failed final digest comparison. Before the final cleanup
change, the client returned failure with those nonzero bytes still present.
It now clears outputs after the last callback returns.

The client now copies the bounded wire before parsing or ZIP-243 hashing.
All six upload passes use this same copy. After the device's final reply, the
client compares the caller's wire with the snapshot before publishing facts
or digest. A changed caller wire triggers the existing best-effort device
erase. Failure outputs are cleared after the erase callback, and the snapshot
is explicitly overwritten before release.

The existing wire cap bounds the extra host heap allocation at 2 MiB.
Clang 22.1.6 with ISO C23, `-O2 -fstack-usage`, on AMD Ryzen 7 PRO 8840U
reported a 664-byte stack frame for the client function. These are host
costs, not Ledger Blue RAM costs. The Blue image and consensus core did not
change.

Commands:

```sh
cmake --build /tmp/z23-blue-standalone-release --parallel 4
ctest --test-dir /tmp/z23-blue-standalone-release --output-on-failure --parallel 4
cmake --build /tmp/z23-blue-standalone-debug --parallel 4
ASAN_OPTIONS=detect_leaks=0 ctest --test-dir /tmp/z23-blue-standalone-debug --output-on-failure --parallel 4
make check-core-seal check-cyclomatic-complexity
ZCL_WINDOWS_ACCEPTANCE_GUARD_SCRATCH=/tmp/z23-blue-shielded-snapshot-wag make lint-fast
```

The final Release suite passed 59/59 tests. A parallel sanitized Debug run
timed out the unchanged Cortex M0 Sapling emulator at its 30-second limit;
that test passed alone in 21.2 seconds. The final serial sanitized Debug
suite passed 59/59 tests in 122.0 seconds. The unchanged core seal and
complexity cap passed after splitting two functions below the cap. All 33
fast lint gates passed. Markdown link and inline path gates passed across
542 documents.

## Limit

The callback test models synchronous mutation. Concurrent unsynchronized
writes are outside the caller contract. A matched replay digest still does
not prove Sapling proof validity, recipients, fee, ownership, or chain state.
The shielded app remains read-only; no physical install was attempted.
