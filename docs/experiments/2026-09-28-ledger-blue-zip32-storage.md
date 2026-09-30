<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Ledger Blue ZIP32 storage separation

Date: 2026-09-29T02:06:38Z (2026-09-28T22:06:38-04:00).
Host CPU: AMD Ryzen 7 PRO 8840U w/ Radeon 780M Graphics.
Host compiler: Clang 22.1.6. ARM compiler: arm-none-eabi-gcc 16.2.0.

## Question

Do the isolated ZIP32 helpers reject partially overlapping secret, output,
and workspace regions before modifying any of them?

## Method and result

The prior contracts required disjoint buffers, but the child and seed bridge
checked only pointer equality and the other helpers did not reject aliasing.
Writing a derived record or clearing a failed result could therefore modify
an overlapping seed or parent key. The shared storage check compares address
distances using `uintptr_t`, without forming an end pointer that could wrap.
Each public helper checks relevant regions before its first write. Rejected
overlap leaves the caller's buffers unchanged; other failure paths retain
their documented clearing behavior. A null seed bridge result now clears a
non-null workspace without calling the BIP32 source.

Host adversarial cases put the seed inside the result, offset an expanded
spending key against its viewing key result, put the tag inside its viewing
key input, and overlap the child, parent, and workspace pairwise. Every case
requires rejection and byte-for-byte preservation of the original buffers.
The Cortex M3 and M0 cases also reject partial master, viewing key, and child
aliasing while preserving the parent record. The existing valid ZIP32 and
Sapling fixtures still pass.

Release CTest passed 60/60 in 131.50 seconds. AddressSanitizer and
UndefinedBehaviorSanitizer Debug CTest passed 60/60 in 89.11 seconds, with
LeakSanitizer disabled for this runner's ptrace environment. The ARM tests
reported `ZIP32 ALIAS <=0x0200`, meaning the added case used at most 512
bytes of test stack. Overall measured stack peaks were 1,404 bytes on
Cortex M3 and 1,512 bytes on Cortex M0, leaving at least 536 bytes in the
2,048-byte emulator reservation. The Release M3 image had 48,408 bytes of
`.text`, 1,360 bytes of `.data`, and 2,708 bytes of `.bss`; M0 had 50,152,
1,364, and 2,720 bytes respectively. These are QEMU test images, not BOLOS
Wallet app sizes or physical Blue measurements.

The 33 `lint-fast` gates passed. The sealed consensus core remained unchanged
(554 files and 80 sections). The cyclomatic complexity ratchet passed at its
existing cap of 15, scanning 63,505 functions. Markdown links and inline
paths passed for 551 documents.

Reproduce with `cmake --build /tmp/z23-blue-standalone-release --parallel 4`
and `ctest --test-dir /tmp/z23-blue-standalone-release --output-on-failure
--parallel 1`; repeat with `/tmp/z23-blue-standalone-debug` and
`ASAN_OPTIONS=detect_leaks=0` for the sanitized build. The device crypto
helpers remain isolated from the Wallet payment approval path. Their
correctness does not establish safe shielded signing on a Blue; device seed
derivation, transaction review, timing analysis, and BOLOS memory use still
need independent evidence.
