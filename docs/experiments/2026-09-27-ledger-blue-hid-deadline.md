<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Ledger Blue HID exchange deadline

Local time: 2026-09-27T23:01:34-04:00.
UTC: 2026-09-28T03:01:34Z.

The C23 HID transport previously passed the complete `timeout_ms` value to
every packet `poll`, including each retry after `EINTR`. A multipart APDU
could therefore exceed the caller's intended wait by the number of reports.
The transport now establishes one `CLOCK_MONOTONIC` deadline before sending
the request and uses its remaining time for every send and receive readiness
wait. The failure path still clears the entire reply buffer and sets its
length to zero.

A socket-backed test sends a valid 80-byte response in two HID reports. With
no delay it succeeds. With roughly 70 ms before each report and a 100 ms
exchange deadline, the corrected transport rejects the incomplete reply and
clears the response. A copy of the previous transport accepted that delayed
test because each report arrived within its separate 100 ms wait. The focused
test passed under Clang 22.1.6 Debug with ASan/UBSan and GCC 16.1.1 Release.

The complete Clang Debug suite passed 52/52 tests and the complete GCC Release
suite passed 50/50. The focused Clang test passed 20 consecutive repetitions.
The staged C23 cyclomatic complexity gate passed 63,076 functions at cap 15.

The installer now admits only the exact pinned Probe, Fixture, and Sign Test
versions that previously opened and exited on the dedicated Blue. It rejects
the pinned Review, Shielded Review, and Wallet images before opening USB.
Read-only authenticated catalog verification remains available for those
pinned images.
This is a release gate, not evidence that simulation predicts every BOLOS or
touchscreen failure. A new image requires separate device qualification
before its install profile is enabled.

The Wallet startup shell previously invoked the EXIT callback for every
finger event, even a touch outside the button. It now decodes the release
coordinates, finds the displayed touchable element, and tests both an outside
touch and a center-button touch on the receive and address-error screens.
This checks the C23 layout and event route; it does not emulate BOLOS touch
forwarding or verify physical touchscreen responsiveness. The startup test
passed under Clang Debug with sanitizers and GCC Release.

The deadline applies to readiness waits. A blocking kernel `read` or `write`
after `poll` could still extend wall-clock time. The test uses local sockets;
physical USB disconnect and HID-driver timing remain unmeasured.
