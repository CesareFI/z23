<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Ledger Blue decryption alias and APDU failure checks

Date: 2026-09-28T10:27:10Z (2026-09-28T06:27:10-04:00).
Host CPU: AMD Ryzen 7 PRO 8840U w/ Radeon 780M Graphics.
Host compiler: Clang 22.1.6.

## Question

Can a caller's accidental buffer alias or missing reply-length pointer
corrupt a Sapling decryption input or leave a transparent payment review live?

## Method and result

The Sapling AEAD opener now checks the output against the key and ciphertext
before clearing output or authenticating. Exact and one-byte-offset aliases
of note and outgoing ciphertext, plus partial overlap with a note key, return
failure without changing input bytes. Disjoint authentication failure still
clears the output. A missing reply-length pointer in the payment APDU handler
now aborts the active review before returning `0x6f00`; the test starts a
valid review and checks that active and verified state are cleared.

Commands from the repository root:

```sh
cmake --build /tmp/z23-blue-standalone-release -j2
ctest --test-dir /tmp/z23-blue-standalone-release -j1 --output-on-failure
cmake --build /tmp/z23-blue-standalone-debug -j2
ASAN_OPTIONS=detect_leaks=0 ctest --test-dir /tmp/z23-blue-standalone-debug -j1 --output-on-failure
```

Release passed 57/57 tests in 16.99 seconds; sanitized Debug passed 57/57 in
15.62 seconds. These suites include Cortex M3 and M0 Sapling arithmetic
models and the M0 Wallet loop model. LeakSanitizer was disabled because the
container blocks its ptrace setup; AddressSanitizer and
UndefinedBehaviorSanitizer remained active. These are software and emulator
results. No new app image was installed on a physical Blue.

The fast lint run passed 32 of 33 gates. Its remaining Windows guard self
test attempted to write into a read-only home directory; the unchanged gate
passed 7/7 self-test cases and scanned 42 source files when its documented
scratch override pointed to writable `/tmp`. The consensus-core seal matched
554 files and 80 sections. Markdown targets passed for 502 documents and
935 local targets; the inline path check scanned 502 documents with zero new
errors.

## Limit

The overlap check protects the decryptor's stated disjoint-buffer contract.
It does not establish device-held viewing authority, transaction binding, or
memo ownership. The uninstalled Wallet still has only 1,024 bytes of apparent
SRAM margin, so Sapling note verification needs reviewed memory reuse before
device integration.
