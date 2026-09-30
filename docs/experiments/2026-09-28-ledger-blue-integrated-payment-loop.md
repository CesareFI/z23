<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Blue payment event-loop integration

Local time: 2026-09-28T04:16:18-04:00

UTC: 2026-09-28T08:16:18Z

The Blue startup shell previously replaced the payment controller with a
stub. The payment screen test used the real controller but called it outside
the app's `answer_command` and `io_event` paths. The new C23 host test links
the real `main.c` and `wallet_payment_device.c` into one process. A valid
payment-begin APDU passes through `answer_command`; the test checks the
`0x9000` reply and the `SEND NEXT CHUNK` screen. A synthetic finger release
inside EXIT reaches the app's exit callback and clears payment state. A
second launch sends USB reset while the payment is visible; the test checks
that the APDU buffer is zero, the payment ends, and the receive screen
appears. EXIT remains reachable after reset.

The test uses deterministic fake account keys and crypto operations. It
does not validate a transaction digest, real BOLOS event delivery, display
pixels, timing, or physical USB behavior. Those are covered separately where
possible; the final device behavior still requires a physical Blue test.

The new `blue-wallet-integrated-loop` case passed under Clang 22.1.6 on an
AMD Ryzen 7 PRO 8840U. The full serial Blue CTest suite passed 54 of 54
cases in Release and 54 of 54 in sanitized Debug. Debug used
`ASAN_OPTIONS=detect_leaks=0` because LeakSanitizer cannot start in this
runner's ptrace environment; AddressSanitizer and UndefinedBehaviorSanitizer
remained active. The cyclomatic complexity gate passed at its unchanged
cap of 15.

Reproduction from the Blue source checkout:

```sh
cmake -S apps/zcl-ledger -B /tmp/z23-blue-release -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/z23-blue-release --target test-blue-wallet-integrated-loop
ctest --test-dir /tmp/z23-blue-release -R '^blue-wallet-integrated-loop$' --output-on-failure
```
