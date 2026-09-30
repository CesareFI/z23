<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Ledger Blue signed-output directory identity

Local time: 2026-09-28T21:41:28-04:00. UTC: 2026-09-29T01:41:28Z.
Compiler: Clang 22.1.6. CPU: AMD Ryzen 7 PRO 8840U w/ Radeon 780M Graphics.

## Question

Does a signing command report an output path that no longer resolves to the
directory where its staged signed transaction will be published?

## Finding

The previous unnamed-file stage pinned an open directory descriptor, but
did not retain the directory path. A test opened an output stage, renamed
that directory, created a replacement directory at the original path, and
committed. The previous source reported success and wrote the signed bytes
under the moved directory, leaving the reported path empty. Compiling the
new regression test against the previous committed output module reproduced
the failure at its expected rejection assertion (exit 134).

The stage now retains the parent path and checks its device and inode
against the opened directory before writing and after syncing the published
entry. On mismatch it refuses the commit and removes an entry it just
published if it can identify that inode. The same rename/replacement test
now passes in Release and sanitized Debug, with no file under either path.
This closes the reproducible mismatch; an external actor with the same
filesystem authority can still rename a directory after the final check.

## Reproduction

```sh
cmake --build /tmp/z23-blue-standalone-release --parallel 4
ctest --test-dir /tmp/z23-blue-standalone-release --output-on-failure --parallel 1
cmake --build /tmp/z23-blue-standalone-debug --parallel 4
ASAN_OPTIONS=detect_leaks=0 ctest --test-dir /tmp/z23-blue-standalone-debug --output-on-failure --parallel 1
make check-core-seal check-cyclomatic-complexity
ZCL_WINDOWS_ACCEPTANCE_GUARD_SCRATCH=/tmp/z23-blue-output-dir-wag make lint-fast
make check-markdown-links check-doc-inline-paths
```

The full Blue suites passed 60/60 in Release and sanitized Debug. The core
seal matched 554 files and 80 sections. The unchanged cyclomatic cap of 15,
all 33 fast lint gates, and both documentation gates passed. The Markdown
checks scanned 550 documents with no new broken targets or paths.

## Limit

This is a host filesystem check. It does not qualify the uninstalled Blue
Wallet image or authenticate the local node's chain state.
