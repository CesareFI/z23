<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Ledger Blue post-review UTXO recheck

Local time: 2026-09-27T07:21:25-04:00

UTC: 2026-09-27T11:21:25Z

## Question

Can the read-only host reject a review result when an input's local UTXO
status or the node tip changes while the user reviews the Blue screens?

## Result

After the Blue reports every output and input digest, the host queries the
node tip, checks every input's unspent status, amount, script hash,
confirmation height, and maturity again, then queries the tip a final time.
Both tips must match the initial block hash and next height. A failed query
or changed input status prevents the host from reporting review success.

The C23 wallet CLI fixture test passed its post-review check with an
unchanged tip and UTXO, and rejected a spent input, a changed script hash,
a changed tip between the two tip queries, and a missing initial tip.
Clang 22.1.6 Debug with AddressSanitizer and UndefinedBehaviorSanitizer and
GCC 16.1.1 Release each passed the focused wallet CLI test 1/1. The
cyclomatic-complexity gate passed 60,772 functions in 4,430 files. CPU:
AMD Ryzen 7 PRO 8840U with Radeon 780M Graphics.
The standalone reviewer built with OpenSSL and PNG discovery disabled,
passed the same fixture, and linked only `libc.so.6` at runtime.

The CLI now keeps transaction bytes, bound previous wires, plan, and digests
in one owned review job. Separate preparation and device steps share one
cleanup path, including partial previous-wire loading failures. The existing
wallet CLI fixture still passed 1/1 under both compilers after the refactor;
the standalone reviewer passed the same fixture and still linked only libc.
The complexity gate passed 60,776 functions in 4,430 files.
The fixture now supplies one valid previous-wire file followed by a missing
second file and requires a clean failure. This exercises the partially
loaded job's shared cleanup path under Clang's sanitizers and GCC Release;
both focused tests passed 1/1.

The CLI now distinguishes a structurally valid v4 transaction containing a
Sapling output from malformed or otherwise unsupported bytes. The C23 fixture
adds one opaque 948-byte Sapling output and a 64-byte binding signature to
an existing transparent transaction. The structural parser accepted that
fixture, and the CLI rejected it with an explicit shielded-unsupported
message. Truncating one binding-signature byte produced the generic
malformed-transaction rejection. Clang sanitizer and GCC Release wallet CLI
tests passed 1/1; the standalone no-OpenSSL reviewer passed the same fixture
and still linked only libc. Neither fixture is a valid proved payment.

At 2026-09-27T08:02:20-04:00 (2026-09-27T12:02:20Z), the CLI fixture also
constructed structurally accepted Sapling spend and Sprout JoinSplit forms.
Each form received the explicit shielded-unsupported error, while truncating
its required binding signature or JoinSplit authorization bytes received the
generic malformed-transaction error. The focused test passed 1/1 with
Clang 22.1.6 Debug and AddressSanitizer/UndefinedBehaviorSanitizer and with
GCC 16.1.1 Release on an AMD Ryzen 7 PRO 8840U. These opaque zero-filled
fixtures test structure and error classification; they are not valid proofs.

At 2026-09-27T08:12:53-04:00 (2026-09-27T12:12:53Z), the host reviewer
opened unsigned and previous-wire files with `O_NONBLOCK` before checking
that they are regular files. A named pipe supplied as an unsigned transaction
now fails promptly with the existing regular-file error. The subprocess
FIFO fixture has a ten-second child timeout to make a renewed blocking open
fail the test instead of hanging it. The focused test passed 1/1 with Clang 22.1.6
Debug and AddressSanitizer/UndefinedBehaviorSanitizer and GCC 16.1.1 Release
on the same AMD Ryzen 7 PRO 8840U.

At 2026-09-27T08:27:22-04:00 (2026-09-27T12:27:22Z), the same fixture
verified that a symbolic link cannot supply either the unsigned transaction
or a previous transaction. Both paths fail before the Blue is opened. The
focused test passed 1/1 with Clang 22.1.6 Debug and sanitizers and GCC
16.1.1 Release on the same CPU.

The updated standalone reviewer also built with GCC using the review-only
CMake target, passed the full CLI fixture with that executable, and has only
`libc.so.6` in its ELF `NEEDED` entries.

At 2026-09-27T08:43:17-04:00 (2026-09-27T12:43:17Z), the shielded fixture
cases were represented as fixed data instead of nested branches. The
cyclomatic-complexity ratchet then passed 60,779 functions in 4,430 files at
the cap of 15. The focused CLI test passed 1/1 again with Clang 22.1.6
sanitizers and GCC 16.1.1 Release on the same CPU.

At 2026-09-27T08:50:21-04:00 (2026-09-27T12:50:21Z), the fixture also
supplied a named pipe as a previous transaction after generating a valid
spend. The reviewer rejected it before opening the Blue. The focused test
passed 1/1 under both compilers, and the complexity ratchet passed all 60,779
functions at the cap of 15.

At 2026-09-27T09:23:15-04:00 (2026-09-27T13:23:15Z), the CLI error was
changed to say that transactions with shielded fields are unsupported. A
structurally accepted v4 fixture with nonzero value balance and no Sapling
spend, output, or JoinSplit now receives that accurate message. Clang 22.1.6
sanitizer and GCC 16.1.1 focused tests passed 1/1, the complexity ratchet
passed 60,779 functions, and the GCC review-only executable passed the same
fixture with only `libc.so.6` linked at runtime. This fixture does not
establish consensus validity.

At 2026-09-27T09:25:18-04:00 (2026-09-27T13:25:18Z), unreadable,
nonregular, empty, or oversized previous-wire files received a file-input
error before digest calculation. Invalid outpoints retained the separate
digest error. The unsigned-wire error now states the nonempty regular-file
and size requirements. The focused CLI test passed 1/1 with Clang 22.1.6
sanitizers and GCC 16.1.1 Release, the complexity ratchet passed 60,779
functions, and the GCC review-only executable passed the fixture while
linking only `libc.so.6`.

At 2026-09-27T09:28:41-04:00 (2026-09-27T13:28:41Z), the child timeout
was limited to the two FIFO regression cases. Other CLI fixture processes
have no artificial ten-second deadline, avoiding a false failure under a
busy proof runner. The focused test passed 1/1 with
Clang 22.1.6 sanitizers and GCC 16.1.1 Release, and the complexity ratchet
passed 60,779 functions at the cap of 15.

At 2026-09-27T09:32:35-04:00 (2026-09-27T13:32:35Z), sparse files one byte
above the Blue's 2 MiB transaction limit were rejected for both unsigned
and previous-wire inputs before the device opened. The focused test passed
1/1 with Clang 22.1.6 sanitizers and GCC 16.1.1 Release, the GCC review-only
executable passed the same fixture, and the complexity ratchet passed 60,779
functions at the cap of 15.

At 2026-09-27T09:38:56-04:00 (2026-09-27T13:38:56Z), empty regular files
were exercised as both unsigned and previous-wire inputs. The CLI rejected
each before the device opened. The focused test passed 1/1 with Clang 22.1.6
sanitizers and GCC 16.1.1 Release, the GCC review-only executable passed the
same fixture, and the complexity ratchet passed 60,779 functions at the cap
of 15.

At 2026-09-27T09:44:51-04:00 (2026-09-27T13:44:51Z), the CLI fixture was
split into bounded C23 helpers for unsigned file inputs, previous-file
versions, node/UTXO failures, and cleanup. The fixture behavior remained
unchanged: Clang 22.1.6 sanitizer and GCC 16.1.1 focused tests passed 1/1,
the GCC review-only executable passed the same fixture, and the complexity
ratchet passed 60,784 functions at the cap of 15.

At 2026-09-27T09:55:56-04:00 (2026-09-27T13:55:56Z), the live
touchscreen-status parser began checking the Blue's reported output count
against the planned transaction, in addition to the current pass, pending
state, acknowledged index, and success status. A mismatched count or stale
status now stops read-only review. Synthetic pending, acknowledged, stale,
truncated, and mismatched-count replies were exercised. The payment review
and CLI fixtures passed 2/2 with Clang 22.1.6 Debug sanitizers and GCC
16.1.1 Release on an AMD Ryzen 7 PRO 8840U. The complexity ratchet passed
60,787 functions at cap 15. This verifies the host parser and simulator;
the updated host path has not been exercised on physical Blue hardware.

At 2026-09-27T09:57:30-04:00 (2026-09-27T13:57:30Z), all 30 Blue/ZCL
host tests passed after full C23 builds with Clang 22.1.6 Debug sanitizers
and GCC 16.1.1 Release on the same AMD Ryzen 7 PRO 8840U. The standalone
GCC reviewer fixture also passed and its ELF runtime dependency list
contained only `libc.so.6`.

## Limit

The post-USB path has not been exercised on physical Blue hardware. Local
RPC responses do not independently prove peer synchronization or wallet
ownership. The separate RPC calls are not an atomic chain snapshot; an
unobserved state change between checks remains possible. Wallet 0.2.16
remains uninstalled and read-only.
