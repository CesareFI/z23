<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Sapling v4 digest field binding

Local time: 2026-09-27T10:11:09-04:00

UTC: 2026-09-27T14:11:09Z

## Result

The existing C23 ZIP-243 implementation matches the published 4,118-byte
[ZIP-243 vector 1](https://zips.z.cash/zip-0243). A new regression mutates
one field at a time in that same vector. Changing the first transparent
output amount, value balance, or the commitment of any of its three Sapling
spends or three Sapling outputs changes the shielded signing digest.
Changing any of the three Sapling spend authorization signatures leaves the
digest unchanged, as ZIP-243
specifies. The existing binding-signature mutation also leaves the digest
unchanged. Fixture layout checks pin the tested offsets to its two public
outputs, three Sapling spends, three Sapling outputs, and absent JoinSplits.

The focused test passed 1/1 under Clang 22.1.6 Debug/ASan/UBSan and GCC
16.1.1 Release on an AMD Ryzen 7 PRO 8840U. The repository complexity
ratchet passed 60,788 functions at cap 15.

At 2026-09-27T10:20:43-04:00 (2026-09-27T14:20:43Z), the mutation check
was extended to all three Sapling spends and all three Sapling outputs in
the published vector. The focused test again passed 1/1 with both Clang
22.1.6 Debug/ASan/UBSan and GCC 16.1.1 Release. The complexity ratchet
passed 60,795 functions at cap 15.

A synthetic mixed transparent/Sapling v4 transaction was also constructed
from the published transparent vector by adding one 948-byte Sapling output
and a 64-byte binding signature. The structural parser accepts it. Mutating
either end of the Sapling output changes the transparent signing digest;
mutating the binding signature does not. The current Blue streaming parser
rejects the same wire, preserving the read-only device's shielded boundary.
This synthetic wire does not contain a valid Sapling proof and cannot be
broadcast. The regression checks digest field binding and explicit rejection,
not proof verification or shielded signing.
At 2026-09-27T10:39:00-04:00 (2026-09-27T14:39:00Z), the full Blue host CTest
suite passed 31/31 with Clang 22.1.6 Debug/ASan/UBSan and 31/31 with GCC
16.1.1 Release on an AMD Ryzen 7 PRO 8840U. The repository complexity
ratchet passed 60,803 functions at cap 15.

## Limit

This test verifies which transaction bytes affect ZIP-243's digest. It does
not validate Sapling proofs, decrypt notes, establish note ownership, verify
the value entering or leaving a shielded pool, or authorize any device
signature. Wallet 0.2.17 still refuses shielded review and cannot sign.
