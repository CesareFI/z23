<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Ledger Blue reviewed-wire assembly binding

Date: 2026-09-28T15:54:59Z (2026-09-28T11:54:59-04:00).
Host CPU: AMD Ryzen 7 PRO 8840U w/ Radeon 780M Graphics.
Host compiler: Clang 22.1.6.

## Question

Can the host assembler accept a structurally valid unsigned wire with a
changed output amount after Blue review and attach a signature record for the
original digest?

## Method

The two-input fixture changes one byte of the second output amount after
capturing the unsigned transaction's SHA-256. The low-level assembler accepts
that changed wire because it can compare the supplied signature record with
the supplied digest but does not have the reviewed wire hash. The new
`blue_payment_host_assemble_reviewed` boundary rejects the changed wire
before writing any output or output length, while accepting the unchanged
wire. It also rejects an output buffer that overlaps the reviewed hash
without modifying that hash. The synthetic fixture CLI checks the reviewed
hash before requesting signatures and calls this checked assembler after
signatures return. A
failure requests a device review abort. The CLI never broadcasts or saves
the assembled bytes.

Commands from the repository root:

```sh
cmake --build /tmp/z23-blue-standalone-release -j2
ctest --test-dir /tmp/z23-blue-standalone-release -j1 --output-on-failure
cmake --build /tmp/z23-blue-standalone-debug -j2
ASAN_OPTIONS=detect_leaks=0 ctest --test-dir /tmp/z23-blue-standalone-debug -j1 --output-on-failure
ZCL_WINDOWS_ACCEPTANCE_GUARD_SCRATCH=/tmp/z23-blue-assembly-wag make lint-fast
make check-core-seal
```

## Measured cost and limits

The serial Release and sanitized Debug suites each passed 57/57 cases. The
changed-amount test confirms that the low-level assembler accepts the
modified fixture while the checked entry point rejects it without writing
output; the unchanged checked path succeeds. All 33 fast lint gates passed in
52.358 seconds at the unchanged cyclomatic cap. The core seal matched 554
files and 80 sections. Markdown links and inline paths passed with no new
errors. LeakSanitizer was disabled because the container blocks its ptrace
setup; AddressSanitizer and UndefinedBehaviorSanitizer remained active.

The assembler's host `.text` grew from 2,436 to 2,820 bytes with Clang
`-std=c23 -O3 -fstack-usage`. The checked entry point uses a 104-byte frame;
the existing assembly entry point remains at 520 bytes. It uses Z23's C23
SHA-256 package, not OpenSSL. No device image or sealed consensus file
changed.

The checked entry point proves only that the host assembles the same
unsigned bytes hashed before review. It does not independently prove that
the Blue displayed them, that prior outputs are spendable, or that a device
signature has been collected. The calling flow must complete the Blue's
review, bound-input verification, and physical signing approval. The Wallet
image is still uninstalled and physical payment signing is unverified.
