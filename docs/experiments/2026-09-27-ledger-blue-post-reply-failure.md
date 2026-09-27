<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Ledger Blue post-reply display failure path

Date: 2026-09-27. Review of the uninstalled Wallet 0.2.2 source found that
`io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, sent)` and the following
`UX_DISPLAY` call were inside the same exception handler as request parsing.
If the display call threw after the reply had already been transmitted, the
handler appended an error status and the next iteration attempted to send
it without a pending request. The SDK's `io_exchange` treats a reply from
`APDU_IDLE` as invalid. This is a source-level failure path, not a measured
cause of the earlier physical 0.2.1 freeze.

Wallet 0.2.3 moves reply transmission and the post-reply screen redraw
outside the request exception handler. A display exception now unwinds to
the app's outer handler instead of creating another APDU response. The same
source review found that a receive exception before a complete APDU could
reach the old error-response path. Version 0.2.3 clears the received length
before each receive and, on that failure, closes the current SDK try context
before rethrowing to the outer app handler. It keeps
the same read-only protocol and has no signing instruction. Two independent
builds with separately patched Blue SDK trees produced identical `.text`
SHA-256 `22aa27cd8043ea05b5d6b976ed757484a2de66659c71c25809887bbd2938f2fa`.
The ARM image has 25,088 bytes of `.text`, 4,236 bytes of `.bss`, and zero
initialized `.data`. The named stack-path maximum is 648 bytes with a
2,048-byte reservation and 512-byte guard. These checks exclude BOLOS
firmware frames and do not establish actual screen or USB behavior.
The repository-wide cyclomatic ratchet passed at cap 15 across 60,401
functions in 4,409 files. The unchanged host test suite passed 23/23 under
Clang Debug with sanitizers and 23/23 under GCC Release; it does not emulate
the BOLOS exception and display loop.

The installer does not accept Wallet 0.2.3. The dedicated Blue's authenticated
catalog last reported zero apps. A physical diagnostic must first prove that
the exact image opens, answers a harmless identity command, renders and exits
reliably, and survives interruption before any transaction upload is tried.
