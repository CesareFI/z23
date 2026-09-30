<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Ledger Blue final approval finger sequence

Date: 2026-09-29T08:27:16-04:00 (2026-09-29T12:27:16Z).
Host: AMD Ryzen 7 PRO 8840U. Compiler: Clang 22.1.6, C23.

## Question

Does the Wallet's finger event gate require a complete touch and release on
the final SIGN ZCL target before invoking its approval callback? The
existing screen test invoked callbacks directly and therefore did not
exercise the gate used by the Blue app loop.

## Method and result

The device UI test now sends Blue-format finger events through
`wallet_payment_finger_allowed`. It invokes the selected button callback
only when the gate accepts the release. A release without touch, a touch on
SIGN ZCL followed by release on NO SIGN, a touch outside either target
followed by release on SIGN ZCL, and a page abort during a finger gesture
all leave signing unapproved. A touch and release on SIGN ZCL approves the
review; EXIT then clears it.

```text
ctest --test-dir /tmp/z23-blue-tail-release --output-on-failure \
  -R '^blue-wallet-device-ui$'
1/1 passed

ASAN_OPTIONS=detect_leaks=0 ctest --test-dir /tmp/z23-blue-tail-debug \
  --output-on-failure -R '^blue-wallet-device-ui$'
1/1 passed
```

The Debug build uses AddressSanitizer and UndefinedBehaviorSanitizer; leak
detection is disabled because LeakSanitizer cannot initialize in this
execution environment. This is a host BOLOS-shim test. It does not establish
physical touchscreen behavior, installed-image behavior, or signing safety
for shielded transactions.
