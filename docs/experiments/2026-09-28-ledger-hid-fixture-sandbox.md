# Ledger HID test fixture in a restricted runner

Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.

The `ledger-hid-io` test uses a local `SOCK_SEQPACKET` pair as its fake HID
device. On 2026-09-28, the restricted test runner returned `EPERM` from the
fixture child's `send()` call. The parent then read end of file and the test
failed before exercising the intended response assertions. This failure also
occurred in a Release build, so it was not caused by a sanitizer.

The fixture now writes reports with `write()`, the same I/O operation used by
the HID transport. The child ignores `SIGPIPE` so the expected timeout case
can observe a closed peer and exit normally. The production HID transport is
unchanged. Signal setup is a separate helper, keeping `fake_device` below
the repository's cyclomatic complexity cap of 15; the complexity gate passed
without changing its baseline.

With Clang 22.1.6, these local checks passed after the fixture change:

```sh
ctest --test-dir /tmp/z23-blue-standalone-release --output-on-failure -j1
ASAN_OPTIONS=detect_leaks=0 ctest --test-dir /tmp/z23-blue-standalone-debug --output-on-failure -j1
```

Each suite passed 53 of 53 tests, including the fixture test and the Cortex-M
QEMU tests. The Debug build retained AddressSanitizer and
UndefinedBehaviorSanitizer. LeakSanitizer could not start in this runner
because it runs under ptrace; this result does not establish leak freedom.

The repository's `make lint-fast` passed all 33 gates with
`ZCL_WINDOWS_ACCEPTANCE_GUARD_SCRATCH=/tmp/z23-wag-scratch`. That variable
placed the Windows guard's temporary fixture in the runner's writable `/tmp`;
the guard's self-tests and production checks were unchanged.
