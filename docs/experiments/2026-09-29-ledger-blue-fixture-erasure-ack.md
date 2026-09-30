<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Ledger Blue synthetic signer erasure acknowledgment

Date: 2026-09-29T07:30:42-04:00 (2026-09-29T11:30:42Z).

## Question

Can the synthetic live signer report a verified signature while the Blue
still holds review state or the final abort response is unknown?

## Result

The fixture CLI previously printed its verified result before a checked
review abort and closed USB on some failures without a final abort request.
It now attempts the protocol-12 abort after every live review/sign attempt.
It reports a verified result only when signing, the abort status, and USB
close all succeed. A rejected abort or transport failure produces no success
report and instructs the owner to restart the app. The CLI wipes its
temporary signature, signed-wire, result, and fixture buffers before exit.

A C23 host transport test injects an acknowledged abort, a rejected status,
and a failed exchange. It confirms that a successful signing result is
accepted only with the acknowledged abort, while a prior failure still
sends an abort. The existing live Wallet controller tests cover review,
signing, and abort under a BOLOS shim. These checks do not establish physical
USB, touch, PIN, or app-opening behavior on a Ledger Blue.

On Clang 22.1.6 and an AMD Ryzen 7 PRO 8840U with Radeon 780M Graphics,
Release passed 63/63 tests in 30.83 seconds. Address- and
undefined-behavior-sanitized Debug passed 63/63 in 35.42 seconds with leak
detection disabled for this traced environment. The 33 fast lint gates,
554-file/80-section consensus-core seal, and cyclomatic cap 15 passed.
No device image or consensus-core source changed.
