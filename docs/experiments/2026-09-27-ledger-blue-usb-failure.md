<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Ledger HID failure response state

Local time: 2026-09-27T00:17:43-04:00

UTC: 2026-09-27T04:17:43Z

## Question

Can a failed HID exchange leave a partly received reply and a stale length
that a Z23 caller might mistake for a successful device response?

## Method and result

The C23 transport now sets the reply length to zero at exchange start and
zeroes the caller's response buffer after send or receive failure. A local
`SOCK_SEQPACKET` peer exercised a complete identity reply, a wrong HID
sequence, a declared 80-byte reply truncated after the first 64-byte report,
and a response timeout. The valid reply passed; all three failures returned
an error, length zero, and an all-zero response buffer. The full CMake suite
passed 19/19 tests with Clang 22.1.6 Debug plus AddressSanitizer and
UndefinedBehaviorSanitizer, and 19/19 with GCC 16.1.1 Release. CPU: AMD
Ryzen 7 PRO 8840U with Radeon 780M Graphics.

This tests the transport state machine over a local packet socket. It does
not verify the Blue's physical USB behavior, power loss, or reconnection.
