<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Ledger Blue previous-wire identity during review

Date: 2026-09-29T02:19:32Z (2026-09-28T22:19:32-04:00).
Host CPU: AMD Ryzen 7 PRO 8840U w/ Radeon 780M Graphics.
Compiler: Clang 22.1.6.

## Question

Can a host callback change a later previous-transaction source after the
owner starts output review, yet still cause Z23 to upload that changed source
before noticing the change?

## Reproduction and correction

The new test changes a previous-wire byte or its declared length during the
first output-upload APDU. On the previous source, the test failed: after the
mutation, Z23 still sent the previous-transaction BEGIN command. The old run
ultimately returned failure, so this test does not establish acceptance of a
wrong signature. It demonstrates that the host
did not keep the source selected at review start bound through the upload.

The host now records each previous source's address, length, and SHA-256
digest before its first device exchange. It checks all of them after output
review and checks each source again before and after uploading its private
copy. A changed source triggers review abort before its previous-transaction
BEGIN command. A two-input case changes the second source during the first
source's upload; the second BEGIN is never sent. The positive one- and
two-input reviews still complete. The caller's existing contract to keep
buffers alive and avoid concurrent writes remains necessary.

## Cost and limits

Compiling the old and new `blue_payment_live.c` with identical Clang Release
flags and `-fstack-usage` measured `blue_payment_live_run_bound` at 3,496
and 4,280 bytes of static host stack, an increase of 784 bytes. The object's
`.text` grew from 6,628 to 7,108 bytes. The identity array has 16 slots;
these are host measurements, not Ledger Blue RAM or app image measurements.
The content comparison adds SHA-256 passes over at most sixteen 2 MiB
previous wires. It does not prove UTXO existence, active chain state, or
device approval. The Blue independently checks the selected previous
transaction's wire ID and ZIP-243 digest before signing.

A C23 CPU-time harness allocated sixteen distinct 2 MiB buffers, filled byte
`i` with `(i + i / 2097152) & 255`, and hashed each buffer four times with
the project's `zsha256` (128 MiB total per round). Five Release rounds took
0.414493, 0.414930, 0.416208, 0.412659, and 0.424178 CPU seconds. This
isolates the worst-case added hashing work on the named host; it excludes
allocation, transaction parsing, USB transfer, and device review latency.

Reproduce the regression with `ctest --test-dir
/tmp/z23-blue-standalone-release -R '^blue-payment-review$'
--output-on-failure` after building `test-blue-payment-review`. The focused
corrected test passed in Release and sanitized Debug. Before the complexity
split, both full local suites passed 60/60; sanitized Debug took 143.19
seconds. After the split, one serial Release run passed 58/60: the unchanged
Cortex M3 and M0 cases timed out at their 15- and 30-second limits under
shared-host load. The same images passed together in a fresh CTest process,
2/2 in 17.91 seconds. A subsequent full serial Release run passed 60/60 in
62.60 seconds without changing either timeout. The final sanitized Debug
suite passed 60/60 in 131.52 seconds with LeakSanitizer disabled for this
runner's ptrace environment. The static stack result is reproducible with
Clang `-O3 -DNDEBUG -std=c23 -Wall -Wextra -Werror
-pedantic -fstack-usage` and the include paths from the target's CMake
`flags.make`.

The first implementation failed the unchanged cyclomatic complexity cap at
M=18. Splitting upload orchestration restored the cap of 15; the final gate
scanned 63,509 functions without a new exception. All 33 fast lint gates,
the 554-file and 80-section consensus-core seal, and both Markdown gates
passed. No consensus-core or Blue device image source changed.
