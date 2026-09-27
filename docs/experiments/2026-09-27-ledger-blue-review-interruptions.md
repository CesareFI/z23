<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Ledger Blue review interruption recovery

Local time: 2026-09-27T05:08:08-04:00

UTC: 2026-09-27T09:08:08Z

## Question

Can a USB disconnection after an APDU reply or during touchscreen review leave a
partly verified transaction available to a later connection?

## Method and result

The C23 host test runs a valid one-input transparent transaction and injects
a connection loss after each APDU reply, including the transaction replay,
output review, previous-output upload, and bound digest exchange. It also
interrupts the first touchscreen continuation. At every interruption, the
device review state matches the explicit abort state, including its failed
replay marker. Replaying an interrupted transaction command without a new
begin command is rejected with no response payload. A fresh complete review
then succeeds and verifies the fee and input binding.

The full local CMake suite passed 26/26 with Clang 22.1.6 Debug and
AddressSanitizer plus UndefinedBehaviorSanitizer, and 26/26 with GCC 16.1.1
Release. CPU: AMD Ryzen 7 PRO 8840U with Radeon 780M Graphics. Test date:
2026-09-27.

This exercises the portable APDU review state and host driver. It does not
exercise BOLOS USB event delivery or establish that the Blue touchscreen
remains responsive after a physical disconnection. The installed read-only
wallet must still pass physical interruption and EXIT tests before signing
is enabled.
