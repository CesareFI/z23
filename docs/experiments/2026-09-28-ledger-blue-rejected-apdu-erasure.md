<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Blue rejected-command buffer erasure

Local time: 2026-09-28T03:25:21-04:00

UTC: 2026-09-28T07:25:21Z

ZCL Wallet 0.3.9 erases the shared APDU buffer and resets the reply length
after a rejected payment command, including a command received before the
account address is available. The same erasure applies when the command
handler returns an invalid reply length. Earlier code aborted the payment
state but could leave the received request or a previous reply in that buffer.
The response framing sends only the status bytes on rejection, so the earlier
behavior did not establish a USB disclosure. The change reduces residual
transaction data in app RAM and makes the reply contract explicit.

The host SDK shell used the same array for request and reply, filled unused
bytes with a nonzero sentinel, and sent malformed signing and non-signing
commands. It also disabled the account-ready flag before a read-only command.
Each rejection left the entire supplied reply capacity zero and reply length
zero. The final source passed the dedicated cyclomatic complexity gate at its
existing cap of 15 without a baseline change.

The serial Blue CTest suite passed 53 of 53 cases in Release and 53 of 53 in
sanitized Debug. Debug used `ASAN_OPTIONS=detect_leaks=0` because
LeakSanitizer cannot start under this runner's ptrace environment;
AddressSanitizer and UndefinedBehaviorSanitizer remained active. An earlier
Release run timed out in `blue-m3-qemu` while lint and Debug tests competed
for CPU time; that test passed in 1.03 seconds when rerun alone, and the
complete Release suite subsequently passed in 14.72 seconds. The final
`make lint-fast` run passed all 33 gates without a threshold or baseline
change. Its 152.749-second wall time exceeded the 75-second soft timing
budget under concurrent load; it did not fail a gate. Markdown links passed
for 491 documents and 935 local targets; inline paths passed for 491
documents with 12 existing baseline entries and zero new failures.

The reviewed Blue SDK built the app with Clang 22.1.6 on an AMD Ryzen 7 PRO
8840U. The Intel HEX SHA-256 was
`1e2e7b9ff759bb893954c73004adf572f3e78753f9f1b6edace857d3c37079f2`;
the extracted `.text` SHA-256 was
`757231e2b862f8785909351e81df117e4f85480d9686d2a8fedcd23a82e5ef03`.
The linked image contained 43,272 text bytes, zero initialized-data bytes,
and 5,120 BSS bytes. The static payment-upload stack path was 1,048 bytes
plus a 512-byte margin; BOLOS frames are outside that calculation.

A second source checkout at signed commit `4a8289e72` built against a
separately patched copy of Blue SDK revision
`3c710b4c62ad847599a2deb0932a50dd1ae4bdff`. Its Intel HEX and
extracted `.text` hashes matched the values above exactly. The SDK patch
SHA-256 was
`4919fd81ba7a3a80880065aaf7898c2edd603b2586f6086a88570c73996019f6`.
Both builds used the same host and compiler toolchain; independent-machine
reproducibility remains unverified.

This test uses a host SDK shell and does not measure physical USB timing or
verify that a BOLOS firmware interrupt cannot expose an earlier reply. The
0.3.9 image has not been installed on a physical Blue.
