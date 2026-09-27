<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Device-derived transparent fee candidate

Date: 2026-09-27. Wallet 0.2.4 could replay and display every output of an
unsigned all-transparent v4 spending transaction, but accepted no previous
wire. A host fee was therefore not a fact verified by the Blue. The
uninstalled Wallet 0.2.5 candidate retains up to 16 input outpoints during
the first replay pass and rejects duplicate outpoints. The app can use them
only after all three spending passes match and every output has been
acknowledged by touchscreen. It then accepts one complete previous wire per
input in order. The C23 streaming selector checks v1-v4 structure and
SHA-256d against the captured outpoint, selects its P2PKH output, and adds
its amount. Only after all inputs are bound does the device subtract its
reviewed output total and display `CALCULATED FEE`, `CHAIN UNCHECKED`, and
`NO SIGNING`. There is no signing command or payment approval.

The host's `zcl-blue-wallet-review --test` still preflights every previous
wire before opening USB. Protocol 10/capability 7 sends the same bytes over
new instructions `26` through `28` and compares the Blue's fee response
with the preflight fee. A mismatched fee, swapped or altered previous wire,
duplicate spending outpoint, premature previous-wire command, malformed
APDU, or interrupted upload ends the attempt and clears the device review
state. The host's preflight does not make the previous outputs unspent,
mature, included in the accepted chain, or owned by the device account.
The supplied branch ID remains unverified. The device discards its replay
digest; no signing permission follows from a displayed fee.

Clang 22.1.6 Debug with address and undefined-behavior sanitizers and GCC
16.1.1 Release passed 24/24 local host tests. The device protocol model
tested one-input and two-input fee calculations, ordered binding, changed
wire and wrong-fee rejection, duplicate outpoints, a failed USB chunk during
previous-wire upload, the touchscreen output gate, and 10,000 malformed APDU
mutations. It does not emulate BOLOS touch,
USB timing, or display exceptions. Two independent C23 ARM builds against
separately patched open-source Blue SDK trees produced identical `.text`
SHA-256 `6c484545832c006cd620401e4e0c14f56f1a88550ccad9c440b988dc8c333a16`.
The image has 29,952 bytes of `.text`, 5,084 bytes of `.bss`, zero `.data`,
and 1,060 bytes outside `.bss` in the 6,144-byte Blue SRAM region. The named
C stack-path maximum is 672 bytes against the 2,048-byte reservation with a
512-byte required margin; the path check now includes previous-wire begin,
feed, and finish. BOLOS firmware frames are not measured. The repository
cyclomatic complexity ratchet passed at cap 15 over 60,510 functions in
4,414 files.

An initial image comparison failed: one old SDK tree lacked the canonical
patch's USB reset/suspend callback in `os_io_seproxyhal.c`. Its image hash
differed. A second SDK tree cloned from the pinned revision and independently
patched with the repository patch matched the canonical tree byte for byte,
and their clean application builds matched at the hash above. The missing
callback matters because the app uses USB reset/suspend to clear review
state. This comparison detects a real SDK provenance difference; it does
not prove BOLOS will deliver those events on the physical Blue.

Wallet 0.2.5 was not installed or pinned for installation. A physical
diagnostic must first prove USB identity, steady receive and review screens,
working EXIT, every output boundary, prior-wire upload, fee display, and
recovery after interruption on the dedicated test Blue. The earlier 0.2.1
physical review stopped answering USB and EXIT, so offline results cannot
establish those behaviors.
