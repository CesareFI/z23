<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Ledger Blue host RPC child deadline

Date: 2026-09-28T18:28:38-04:00; UTC: 2026-09-28T22:28:38+00:00.
Host: AMD Ryzen 7 PRO 8840U with Radeon 780M Graphics. Compiler: Clang
22.1.6. The Ledger Blue device image is unchanged.

The host review uses a local RPC child to check the mainnet chain tip and
unspent outputs. The prior reader bounded time until stdout closed, then
called blocking `waitpid` with no deadline. A child could send valid JSON,
close stdout, and remain alive. The host would wait for the child rather
than finish or reject the check.

The regression child sent a valid chain-tip reply, flushed and closed its
stdout, then slept for nine seconds. Before the correction, the test failed
after 16.13 seconds: the host accepted that child as successful after its
sleep. The first existing silent-child case accounts for approximately
seven seconds of that test run. The corrected helper gives reading and
child exit one shared seven-second monotonic deadline. It rejects and kills
an unfinished child, waits for its termination, clears received reply bytes,
and sets the declared reply length to zero. The adversarial test also checks
that no direct child remains to reap. The corrected focused Release and
sanitized Debug tests passed in 14.05 and 14.18 seconds, respectively.

The complete Release suite passed 59/59; the AddressSanitizer and
UndefinedBehaviorSanitizer Debug suite passed 59/59 with
`ASAN_OPTIONS=detect_leaks=0`. This is host C23 code; the Wallet 0.3.33 ARM
image, stack, SRAM, and install block are unchanged.

```sh
cmake --build /tmp/z23-blue-standalone-release --parallel 4
ctest --test-dir /tmp/z23-blue-standalone-release --output-on-failure --parallel 4
cmake --build /tmp/z23-blue-standalone-debug --parallel 4
ASAN_OPTIONS=detect_leaks=0 ctest --test-dir /tmp/z23-blue-standalone-debug \
  --output-on-failure --parallel 4
```

The child deadline does not verify that the RPC executable, its local node,
or the Blue's chain view is trustworthy. The sandbox cannot inspect the
Blue's USB interface, so this test does not identify the earlier physical
Wallet startup freeze. The device install block remains in place.
