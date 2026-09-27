<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Input-specific ZIP-243 digests after Blue prevout binding

Date: 2026-09-27. The read-only Wallet 0.2.5 candidate verified previous
transaction wires and displayed their resulting fee, but discarded its
transaction replay digest. A future signer must bind each input digest to
both the complete reviewed spending wire and that input's exact previous
output script and amount. Wallet 0.2.6 retains the third-pass ZIP-243 outputs
hash and captures every input sequence alongside its outpoint. Once the
three-pass spending review and a previous-wire SHA-256d check succeed, it
computes ZIP-243 SIGHASH_ALL for that input using the selected P2PKH script,
amount, captured outpoint and sequence, and supplied branch ID. Instruction
`28` returns the 32-byte digest after its fee fields. Z23 independently
computes every hash-bound input digest before opening USB and rejects any
device mismatch. A failed digest comparison sends a review abort.

The branch ID is supplied by the host and is not checked against the active
ZCL consensus branch. The device also lacks chain inclusion, unspentness,
maturity, input ownership, and final payment approval. Its fee screen says
`CHAIN UNCHECKED`, `BRANCH UNCHECKED`, and `NO SIGNING`; there is no payment
signature command. The returned digests are review evidence, not signing
authority.

The C23 host model tested one- and two-input transactions, reordered or
changed previous wires, a wrong fee, a wrong expected digest, duplicate
outpoints, a failed USB chunk, invalid command order, and 10,000 malformed
APDU mutations. A direct replay test compared the post-review digest with
the full-wire host ZIP-243 result and checked that an invalid script leaves
the output buffer unchanged. Clang 22.1.6 Debug with address and undefined
behavior sanitizers passed 24/24 tests; GCC 16.1.1 Release passed 24/24.
These are host model tests and do not emulate BOLOS USB or touch timing.

Two clean builds from independently patched copies of Ledger's open-source
Blue SDK at revision `3c710b4c62ad847599a2deb0932a50dd1ae4bdff`
produced identical `.text` SHA-256
`fdecd676292997d1733e992d6baad0ceda8b9949513404188d21d54f1e8bb68d`.
The image has 30,464 bytes of `.text`, 5,180 bytes of `.bss`, and zero
`.data`. Its `.bss` includes the 2,048-byte reserved stack and leaves 964
bytes in the 6,144-byte app SRAM region. The named C stack-path maximum is
680 bytes, including a conservative path through previous-wire finish and
the bound digest, against the required 512-byte stack margin. BOLOS firmware
frames are excluded from this accounting. The installer does not pin this
image. The repository cyclomatic complexity ratchet passed at cap 15 over
60,513 functions in 4,414 files. Wallet 0.2.6 has not been installed or
physically tested.

Before physical payment testing, the Blue must reliably open and exit,
respond through every output and previous-wire boundary, show the fee,
survive USB interruption, and recover without freezing. The earlier Wallet
0.2.1 physical review stopped answering USB and EXIT; these offline tests do
not establish that fault is fixed.
