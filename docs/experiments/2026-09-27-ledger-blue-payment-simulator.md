<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Transaction-driven transparent payment simulator

Local time: 2026-09-27T01:23:24-04:00

UTC: 2026-09-27T05:23:24Z

## Question

Can the host preview derive its output pages from unsigned transaction bytes,
exercise each output pause and acknowledgement, and withhold all pages if
the final transaction replay is invalid?

## Result

The C23 host simulator runs the same bounded review controller used by the
unit test. It uploads the unsigned transaction for all three passes and
simulates an acknowledgement each time an output becomes pending in pass
three. It retains at most 16 formatted output screens. Only after the
complete third pass matches the first pass's SHA-256 commitment does it
return screens to the PNG renderer. This is a simulated touch; no hardware
approval occurs.

The one-input/two-output test fixture produces two 320 × 480 PNGs with full
P2PKH and P2SH addresses and exact one- and two-ZCL amounts. Both rendered
pages were inspected at original size. A malformed P2SH script and a
truncated transaction return zero pages. An invalid branch ID argument is
rejected before reading the file. Reproduce the preview with:

```sh
build/zcl-ledger/test-blue-payment-review /tmp/zcl-unsigned-fixture.bin
build/zcl-ledger/zcl-blue-payment-sim 76b809bb /tmp/zcl-unsigned-fixture.bin /tmp/zcl-payment
build/zcl-ledger/zcl-blue-payment-sim --dark 76b809bb /tmp/zcl-unsigned-fixture.bin /tmp/zcl-payment-dark
```

Clang 22.1.6 Debug with address and undefined behavior sanitizers and GCC
16.1.1 Release each passed 22/22 host tests on AMD Ryzen 7 PRO 8840U. The
repository cyclomatic gate passed at cap 15.

## Limit

The caller supplies the consensus branch ID; the simulator does not verify
it against a ZCL node. The empty scriptCode and zero spent amount passed
to the final replay check create an unused diagnostic digest, never a
payment signature. The harness does not emulate BOLOS USB, touchscreen
timing, the Blue framebuffer, key derivation, or Sapling proof validation.
Screens show transparent outputs only and explicitly state no signing.
