<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->
# Ledger Blue host reply stability during signature verification

Date: 2026-09-28T18:58:23-04:00 / 2026-09-28T22:58:24Z.

## Question

Can the host return a signature record whose reply or digest bytes changed
after the cryptographic verification callback accepted them?

## Experiment and result

The single-reply verifier test uses a valid signed APDU frame and a callback
that changes one byte after checking the signature. The original function
returned success after the reply's DER byte changed, demonstrating a result
that no longer represented the bytes verified. The Release test failed in
0.39 seconds. Additional cases change the caller's expected digest or key
hash, or change the callback-facing DER argument. All four cases now reject
the reply and leave the result record zeroed.

The host verifier now retains an unexposed copy of the reply, digest, and
expected key hash. Hash and signature callbacks receive separate working
copies. A successful result requires the caller's inputs and callback copies
to match the retained bytes after the callbacks return. The returned record
is copied only from the retained values. The protocol, Blue app image, and
consensus core are unchanged.

Clang 22.1.6, ISO C23, `-O2 -fstack-usage`, on AMD Ryzen 7 PRO 8840U reported
a static 504-byte stack frame for `blue_payment_host_verify`. This is host
stack. It is not a measured Ledger Blue RAM cost.

Commands:

```sh
cmake --build /tmp/z23-blue-standalone-release --parallel 4
ctest --test-dir /tmp/z23-blue-standalone-release --output-on-failure --parallel 4
cmake --build /tmp/z23-blue-standalone-debug --parallel 4
ASAN_OPTIONS=detect_leaks=0 ctest --test-dir /tmp/z23-blue-standalone-debug --output-on-failure --parallel 4
make check-core-seal check-cyclomatic-complexity
ZCL_WINDOWS_ACCEPTANCE_GUARD_SCRATCH=/tmp/z23-blue-host-verify-wag make lint-fast
```

The final Release and sanitized Debug suites each passed 59/59 tests. The
core seal, unchanged cyclomatic complexity cap, and all 33 fast lint gates
passed. Markdown link and inline path gates passed across 540 documents.

## Limit

This establishes stability of the verifier's inputs during synchronous
callbacks. It does not authenticate the hash or signature callback
implementations themselves, cover concurrent unsynchronized writes, or prove
physical-device behavior. The earlier Blue app freeze remains unresolved;
no physical install was attempted.
