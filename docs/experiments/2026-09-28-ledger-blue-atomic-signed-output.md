<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Ledger Blue signed-output publication

Local time: 2026-09-28T21:16:28-04:00. UTC: 2026-09-29T01:16:28Z.
Compiler: Clang 22.1.6. CPU: AMD Ryzen 7 PRO 8840U w/ Radeon 780M Graphics.

## Question

Can the transparent signing command avoid leaving an empty or partially
written transaction at its requested output path if the host dies while the
Blue is reviewing a payment?

## Finding

The command now opens an unnamed `O_TMPFILE` inode in the destination
directory before review. It writes the verified signed transaction only
after the Blue acknowledges review erasure, syncs the complete file, creates
the final name with `linkat(AT_EMPTY_PATH)`, and syncs the directory.
The link operation refuses an existing destination without replacement.
The file mode is no broader than 0600. A failed or interrupted review does
not expose an empty or partial file under the requested name.

The new test forks a child that writes partial stage bytes and exits without
cleanup; the parent observes no final file. It also verifies a committed
file's exact bytes and permissions, refuses an existing symlink, preserves a
file created between stage opening and commit, rejects a second commit, and
rejects oversized output. The stage keeps only two file descriptors and a
copy of the destination leaf in host memory; signed-wire buffer size remains
bounded by the existing 2 MiB limit.

The destination filesystem must support Linux `O_TMPFILE` and
`linkat(AT_EMPTY_PATH)`. The local `/tmp` filesystem supported both in a
separate C23 probe. Failure to open an unnamed file stops before review;
failure to link it after signing leaves no output. A process death after
the atomic link can leave a complete signed
file even if the command did not report success; it cannot leave a partial
file at that name.

## Reproduction

```sh
cmake -S apps/zcl-ledger -B /tmp/z23-blue-standalone-release -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/z23-blue-standalone-release --parallel 4
ctest --test-dir /tmp/z23-blue-standalone-release --output-on-failure --parallel 1
cmake -S apps/zcl-ledger -B /tmp/z23-blue-standalone-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/z23-blue-standalone-debug --parallel 4
ASAN_OPTIONS=detect_leaks=0 ctest --test-dir /tmp/z23-blue-standalone-debug --output-on-failure --parallel 1
make check-core-seal check-cyclomatic-complexity
ZCL_WINDOWS_ACCEPTANCE_GUARD_SCRATCH=/tmp/z23-blue-atomic-output-wag make lint-fast
make check-markdown-links check-doc-inline-paths
```

Release and sanitized Debug each passed all 60 Blue tests, including the
new signed-output test. The core seal matched 554 files and 80 sections.
The complexity cap of 15 passed without changing its baseline.
All 33 fast lint gates passed. The Markdown link and inline-path checks
scanned 549 documents with no new broken targets or paths.

## Limit

This test covers host file publication, not a physical Ledger Blue. No
current Wallet image has passed the physical startup and signed-catalog
checks required before a real transaction. Output publication alone does
not authenticate the installed app or independently prove chain state.
