<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Blue payment touch hit-box simulation

Local time: 2026-09-28T03:33:38-04:00

UTC: 2026-09-28T07:33:38Z

The Blue wallet device UI test now activates each displayed payment button
through its center coordinate and the visible BAGL touch rectangle. This
replaces direct callback calls for ordinary taps. Stale callback tests still
invoke their saved callbacks deliberately to model delayed firmware delivery.
The new check also releases a touch in the gap between CONTINUE and EXIT on
an output page. It leaves the page, pending output, and exit count unchanged.
Every displayed page checks that active touch rectangles are at least 44 by
44 pixels and do not overlap.

The targeted `blue-wallet-device-ui` case passed in Clang 22.1.6 Release and
Debug with AddressSanitizer and UndefinedBehaviorSanitizer on an AMD Ryzen 7
PRO 8840U. The final serial Blue CTest suite passed 53 of 53 cases in Release
and 53 of 53 in sanitized Debug. Debug used
`ASAN_OPTIONS=detect_leaks=0` because LeakSanitizer cannot start under this
runner's ptrace environment; AddressSanitizer and UndefinedBehaviorSanitizer
remained active. The complexity gate passed at its unchanged cap of 15. Device
source, image, and version 0.3.9 are unchanged by this test refinement.

This host model checks coordinate hit boxes and callbacks. It does not emulate
the Blue firmware's touch event queue or prove that a physical screen remains
responsive under USB load. The current app image remains uninstalled.
