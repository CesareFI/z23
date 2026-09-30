<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# One Sapling replay binds spend and output captures

Local time: 2026-09-28T04:44:08-04:00

UTC: 2026-09-28T08:44:08Z

The C23 shielded replay can now capture an indexed Sapling spend `rk` and
an indexed Sapling output's `cv`, `cm`, `epk`, and ciphertexts during the same
six-pass upload. Previously those captures required separate replay sessions.
The result is provisional until all six complete wires have equal SHA-256
commitments and the ZIP-243 digest is derived. Rejection clears both output
buffers. Successful finish erases the intermediate replay state while leaving
the verified digest and captures with the caller.

The test uses the committed 1,425-byte consensus-accepted simnet transaction.
One replay yielded the recorded ZIP-243 digest, the transaction's sole `rk`,
and the exact sole output fields. An out-of-range output index was rejected
and cleared both buffers. Altering `rk` during pass two rejected the replay
and cleared both provisional buffers. These checks use public fixture data;
they do not grant payment approval or supply device keys.

On an AMD Ryzen 7 PRO 8840U, Clang 22.1.6 passed the complete 54-case Blue
CTest suite in Release and AddressSanitizer/UndefinedBehaviorSanitizer Debug.
Debug used `ASAN_OPTIONS=detect_leaks=0` because LeakSanitizer cannot start
in this runner's ptrace environment. The shielded APDU fuzzer completed
100,000 sanitized inputs with no failure, using the existing review corpus.

The reviewed Ledger Blue SDK at `3c710b4c62ad847599a2deb0932a50dd1ae4bdff`
with patch SHA-256
`4919fd81ba7a3a80880065aaf7898c2edd603b2586f6086a88570c73996019f6`
built the read-only shielded review image with Clang 22.1.6 and ARM GCC
16.2.0. The Intel HEX SHA-256 was
`5c2554a136ccb36ea91d981cc933a88ab900feb6c4e9cbf3ead697dfff22f848`;
extracted `.text` SHA-256 was
`630e5c35336f564e05e4ea4131b670f407b8d470969ec66b04e4308ddd5d72c6`.
The image used 30,976 bytes of `.text`, no `.data`, and 4,320 bytes of
`.bss`. The largest modeled shielded finish stack path was 840 bytes of a
1,536-byte gate, leaving the required 512-byte margin. SDK and BOLOS stack
frames are outside this local model.

A second source checkout at signed commit `7be725b80` used a separately
patched copy of the same SDK revision. Its Intel HEX and extracted `.text`
SHA-256 values matched those above byte for byte. Both builds used one host
and toolchain; independent-machine reproducibility remains unverified. The
final source passed all 33 `lint-fast` gates, the 554-file/80-section core
seal, and Markdown checks for 496 documents with zero new inline-path
failures.

The installed Blue app does not use the combined-capture API or sign
shielded transactions. Shielded recipient, value, memo, change, and fee
verification and a final device approval remain necessary before connecting
the replay to a signer. The image has not been installed on physical hardware.
