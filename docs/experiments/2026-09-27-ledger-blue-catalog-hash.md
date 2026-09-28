<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->
# Ledger Blue catalog hash parsing

The format-1 secure manager app-list record has a one-byte length, four-byte
flags, a 32-byte code/data hash, a 32-byte application hash, a one-byte name
length, and the name. Z23 previously discarded both digests and did not check
the record length. It now retains both digests and rejects a record whose
declared length differs from the parsed length. `--ca-list` prints the full
application hash beside each name.

The field layout was checked against LedgerHQ/blue-loader-python commit
`a37ef7fe0a7ee7d01882c91afe87b58c03927317`,
`ledgerblue/hexLoader.py`, `HexLoader.listApp`. The loader labels the fields
`hash_code_data` and `hash`. Its `hashApp.py` describes the application hash
as a digest of target and creation parameters plus installed image bytes;
therefore it cannot be compared with the code file's plain SHA-256 pin.

The C23 fixture checks both digests, flags, name, truncated records, bad
lengths, invalid names, capacity, and unsupported format. A catalog result is
evidence of the manager's response within its authenticated channel. It does
not show that the icon is visible, that the app can open, or that an APDU runs.
Physical image comparison requires a pinned installation-hash calculation for
the Blue firmware format and a live catalog result from the dedicated device.

The installer now offers `--ca-verify CA_KEY_FILE app.bin`. It checks the
binary against a reviewed SHA-256 pin before opening USB, constructs the same
create command and parameters used by installation, calculates the
target/version/create/payload application hash, and compares it with the
authenticated manager catalog. It requires exactly one entry for the app
name and rejects a different hash or a duplicate name. The calculation is
shared with custom-CA signing, and its formula follows the loader's legacy
Blue hash path. A live comparison with the dedicated Blue is still required
to verify the firmware reports that same hash for an installed image.
At 2026-09-27T10:52:46-04:00 (2026-09-27T14:52:46Z), the focused catalog
and pre-USB image-input tests passed 2/2 under Clang 22.1.6
Debug/ASan/UBSan and 2/2 under GCC 16.1.1 Release on an AMD Ryzen 7 PRO
8840U. The repository complexity ratchet passed 60,808 functions at cap 15.

At 2026-09-27T10:39:00-04:00 (2026-09-27T14:39:00Z), the complete Blue host
suite passed 31/31 under Clang 22.1.6 Debug/ASan/UBSan and 31/31 under GCC
16.1.1 Release on an AMD Ryzen 7 PRO 8840U. The repository cyclomatic
complexity ratchet passed 60,803 functions at cap 15. The catalog output
format itself has not yet been exercised against the physical Blue.

At 2026-09-27T10:44:00-04:00 (2026-09-27T14:44:00Z), the catalog parser
completed 100,000 in-process libFuzzer cases under Clang 22.1.6 with
AddressSanitizer and UndefinedBehaviorSanitizer on an AMD Ryzen 7 PRO 8840U.
The seed corpus contained a valid one-app format-1 response and an empty
format-1 page. The run ended without a sanitizer finding; it does not prove
that every possible catalog response has been exercised.
