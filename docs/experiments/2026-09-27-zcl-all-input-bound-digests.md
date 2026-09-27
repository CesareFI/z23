<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# All-input transparent digest binding

Local time: 2026-09-27T01:57:09-04:00

UTC: 2026-09-27T05:57:09Z

## Question

Can the C23 host return a transparent fee and all input ZIP-243 digests only
after every supplied previous transaction is bound to its spending outpoint?

## Method and result

`zcl_tx_transparent_bound_digests` first runs the existing complete-transaction
preflight. It then derives each input digest from its SHA-256d-bound previous
transaction into temporary storage. Only after all checks pass does it copy
the facts and digests to the caller. The test exercises one and two inputs,
compares the selected digest with the direct ZIP-243 result, rejects an
insufficient digest buffer, and verifies that a changed previous transaction
leaves caller outputs unchanged. Clang Debug with address and undefined
behavior sanitizers and GCC Release each passed the 22-test local suite.

This operation establishes consistency with supplied transaction bytes. It
does not establish UTXO existence, chain inclusion, maturity, ownership, or
the active consensus branch. The Blue app does not yet run this preflight or
sign a payment.
