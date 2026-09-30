<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Ledger Blue live review input isolation

Date: 2026-09-28T15:27:48Z (2026-09-28T11:27:48-04:00).
Host CPU: AMD Ryzen 7 PRO 8840U w/ Radeon 780M Graphics.
Host compiler: Clang 22.1.6.

## Question

Can a host callback alter the unsigned transaction or planned review text
after planning, while the live Blue review driver reports success for a
different transaction or display?

## Method

An in-process Blue APDU model changes a transaction byte after the identity
reply, during transaction upload, or during previous-transaction upload. It
also changes a planned amount during replay and supplies a forged amount
before USB access. Two red tests on the original driver returned success
after an identity-time transaction mutation and a previous-upload mutation.
The failing assertions are recorded in
`/tmp/z23-blue-live-snapshot-red-run.log` and
`/tmp/z23-blue-bound-mutation-red-run.log` on this host.

The driver now stores the unsigned wire and a freshly parsed review plan in
one bounded host allocation. It uploads only that private wire copy. It
rejects a caller plan that differs in any active review field, checks the
caller's wire hash after identity and at review completion, and requests
review abort if the input changed after BEGIN. Bound review freezes expected
input digests and rechecks the caller's wire, plan, and digests before and
after each previous-transaction stream. Planning rejects plan storage that
overlaps the input wire before clearing any output bytes.

Commands from the repository root:

```sh
cmake --build /tmp/z23-blue-standalone-release -j4
ctest --test-dir /tmp/z23-blue-standalone-release -j1 --output-on-failure
cmake --build /tmp/z23-blue-standalone-debug -j4
ASAN_OPTIONS=detect_leaks=0 ctest --test-dir /tmp/z23-blue-standalone-debug -j1 --output-on-failure
ZCL_WINDOWS_ACCEPTANCE_GUARD_SCRATCH=/tmp/z23-blue-live-snapshot-wag make lint-fast
make check-core-seal
```

## Measured cost and limits

The serial Release and sanitized Debug Blue suites each passed 57/57 cases.
LeakSanitizer was disabled because the container blocks its ptrace setup;
AddressSanitizer and UndefinedBehaviorSanitizer remained active. The tests
reject all injected mutations, preserve a responsive simulated device after
abort, and accept the unchanged review path. All 33 fast lint gates passed
at the existing cyclomatic cap of 15. The lint wall time was 212.997 seconds,
above its 75-second soft budget under concurrent host testing; no gate was
skipped. The core seal matched 554 files and 80 sections. Markdown links and
inline paths passed with no new errors.

`sizeof(blue_payment_live_plan)` is 2,592 bytes. The wire parser permits at
most 2,097,152 bytes, so the new host allocation is at most 2,099,744 bytes.
The call fails before USB access if allocation or independent parsing fails.
Clang `-std=c23 -O3 -fstack-usage` measured the runner frame at 312 bytes
before this change and 328 bytes after it; the bound-review frame grew from
344 to 3,480 bytes because it freezes the plan and digests. The host object
`.text` grew from 4,992 to 5,772 bytes. These are host measurements, not
Ledger Blue flash or SRAM measurements. No device image changed.

The APDU model and host sanitizers test sequential callback mutation. They do
not establish safety if another thread concurrently writes the caller's C
objects, and they do not establish behavior on physical Blue firmware. The
previous-transaction wire remained caller-owned while it was streamed in
this experiment. The follow-up
[previous-wire experiment](2026-09-28-ledger-blue-previous-wire-snapshot.md)
freezes each wire before upload. The Wallet image remains uninstalled pending
physical startup verification.
