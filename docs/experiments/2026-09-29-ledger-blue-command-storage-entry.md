<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->
# Ledger Blue command-entry storage isolation

## Question

Can an early signing rejection write through an aliased reply pointer into
device-owned account state before the low-level APDU storage checks run?

## Experiment and correction

The device UI test approved a payment, changed its stored input digest,
and supplied the device account-hash buffer as the signing reply. Wallet
0.3.42 returned from its early digest-mismatch rejection after erasing the
account hash while leaving account readiness set. The new regression failed
its required `0x6f00` invalid-storage assertion.

Wallet 0.3.43 checks APDU request, reply, and reply-length ranges against
the Wallet workspace, hash contexts, account hashes, display buffers, and
control flags before any command path can write a rejection. The request and
reply may still share the external APDU buffer; reply-length storage must
be disjoint. An invalid layout aborts the payment and returns `0x6f00`
without writing through the supplied pointers. The regression now confirms
that the account hash is unchanged, account readiness remains true, and
the signer is not called.

The ARM stack checker requires the storage-check frame and reports its
modeled path. Removing `overlaps_wallet_display` from a copy of its `.su`
inputs makes the gate fail with
`Missing stack frame: overlaps_wallet_display`.

This is a C API storage fault; the USB host sends bytes and cannot choose
device pointers. The test verifies that the Wallet boundary rejects an
invalid in-process caller without corrupting device-owned state.

## Measurements and limits

At 2026-09-29T03:26:41-04:00 (2026-09-29T07:26:41+00:00), Clang 22.1.6
on an AMD Ryzen 7 PRO 8840U built Wallet 0.3.43 against Blue SDK commit
`3c710b4c62ad847599a2deb0932a50dd1ae4bdff` with reviewed SDK patch
SHA-256
`4919fd81ba7a3a80880065aaf7898c2edd603b2586f6086a88570c73996019f6`.
The image uses 53,504 bytes of `.text`, zero `.data`, and 5,120 bytes of
`.bss` including the 2,048-byte reserved stack. The stack checker reported
572 bytes for command-entry storage checking, 912 bytes for approval touch,
1,096 bytes for signer derivation, and 1,128 bytes for its largest modeled
payment path, with a required 512-byte margin. BOLOS frames are excluded.
The `.text` SHA-256 was
`30673ea16dddb71973da4ef10a966035b8d8188fb639e5a24b475419f20df06e`;
the Intel HEX SHA-256 was
`717951cd0c9f19fc4647f70e9e4fc28b71feb30312b19bf8df5a30339eb816b1`.

Release and sanitized Debug CTest each passed 60/60 cases. Debug used
`ASAN_OPTIONS=detect_leaks=0 LSAN_OPTIONS=detect_leaks=0` because this
environment's LeakSanitizer fails under process tracing before test
execution; address and undefined-behavior checks remained active. The
image has not been installed on a physical Blue.

A fresh checkout of signed commit
`7d83b0d3d56c2f856f58dfbaa2b583f0c325f99d` built against a second
separately patched copy of the pinned SDK produced the same `.text` and HEX
hashes and section sizes shown above. Its stack checker reported the same
paths. On the source, all 33 fast lint gates, the unchanged consensus-core
seal, cyclomatic complexity cap 15, and both Markdown gates passed.
