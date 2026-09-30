<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Ledger Blue suspension after a partial signing sequence

Date: 2026-09-29T07:39:45-04:00 (2026-09-29T11:39:45Z).

## Question

After the first signature of a two-input transparent transaction, does a
USB suspend event clear the second signing approval and temporary APDU state?

## Result

The integrated BOLOS-shim test now injects USB reset, USB suspend, and
approval timeout separately after the first signature. Each case checks
that the APDU buffer is zero, payment state and the approval timer are
cleared, the receive screen returns, and a second signing request fails
without invoking the signer. The Release and AddressSanitizer/UBSan Debug
focused tests passed. This is a simulated event path; it does not establish
Ledger Blue firmware timing or physical USB recovery.

The test uses the existing Wallet app loop and signer stub. It changes no
device image, signing policy, or consensus code. The next physical check is
to interrupt a reviewed synthetic two-input signing session at the USB
port, reconnect, and verify that the remaining signature cannot be obtained
without a new complete review and touchscreen approval.
