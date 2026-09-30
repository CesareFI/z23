<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Ledger Blue candidate Sapling signer failure cleanup

Date: 2026-09-29T07:45:45-04:00 (2026-09-29T11:45:45Z).

## Question

Does the isolated device-derived Sapling SpendAuth wrapper erase partial
signatures and derivation workspace when a device operation fails after
writing secret material or the PIN becomes invalid between steps?

## Result

The C23 device-wrapper test now injects partial-output failures in master
derivation, account-child derivation, entropy acquisition, and SpendAuth
signing. It also revokes the PIN immediately after each of the first three
operations. Each of the seven cases requires a zeroed signature and complete
workspace, and checks that no later stage runs after the failure. The focused
test passed in Release and AddressSanitizer/UBSan Debug.

After the test change, the complete Clang 22.1.6 C23 Release suite passed
63/63 in 30.69 seconds. The complete AddressSanitizer/UBSan Debug suite
passed 63/63 in 35.84 seconds with leak detection disabled in this traced
environment. These are local host and emulator results, not physical-device
checks.

This candidate remains excluded from the installed Wallet image by its
compile-time barrier. Stubbed BOLOS calls establish wrapper control flow and
cleanup only; they do not establish seed compatibility, target timing,
side-channel resistance, physical PIN behavior, or payment approval.
