<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Blue shielded capture aliasing

Local time: 2026-09-28T05:28:38-04:00

UTC: 2026-09-28T09:28:38Z

The six-pass Sapling replay stores provisional selected output bytes and a
spend `rk` outside its bounded state. Previously, beginning a replay cleared
these buffers before checking whether they overlapped the replay state, each
other, or the hash callback descriptor. A malformed caller layout could erase
the callback before use or overwrite replay bookkeeping. Finish could also
write the digest into replay state before erasing that state, return success,
and leave the caller with an erased digest.

The C23 replay now rejects fixed-size buffer overlaps before beginning any
capture. It also rejects a hash context whose base address falls inside a
capture, replay state, or callback descriptor. Finish rejects overlapping
fact and digest outputs, and outputs that overlap replay state or provisional
captures. A rejected finish aborts the replay and erases provisional data.
The full size of a callback-owned hash context is not carried by the generic
hasher interface; callers must still ensure that its entire allocation is
disjoint. The memo inspector likewise rejects an overlapping input and
result without erasing the input.

Tests pass overlapping replay state, output capture, spend key, callback
descriptor, callback context base, facts, digest, and memo input/result
buffers. They verify that a rejected begin leaves the supplied bytes
untouched and that a rejected finish erases provisional captures. Valid
six-pass consensus and synthetic replay vectors continue to pass.

Clang 22.1.6 on an AMD Ryzen 7 PRO 8840U passed 55/55 Blue CTest cases in
Release and 55/55 in sanitized Debug. The first Debug run timed out in the
Cortex-M3 and Cortex-M0 QEMU cases under shared-host load. Both cases passed
when rerun alone, and the subsequent full serial Debug suite passed 55/55.
Debug used `ASAN_OPTIONS=detect_leaks=0` because LeakSanitizer cannot start
under this runner's ptrace configuration; AddressSanitizer and
UndefinedBehaviorSanitizer remained active.

The memo read/send target is one Wallet app. A read flow needs device-derived
viewing authority, note decryption, note-commitment and ephemeral-key checks,
and chain context before the Blue displays memo text as a trusted fact. A send
flow needs every outgoing output and memo bound to the exact transaction,
then a distinct final touch approval. Until those checks exist, no memo text
or host-supplied label can authorize Sapling signing.
