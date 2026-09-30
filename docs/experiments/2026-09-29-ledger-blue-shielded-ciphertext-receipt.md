<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Ledger Blue shielded ciphertext receipt binding

Date: 2026-09-29T08:10:40-04:00 (2026-09-29T12:10:40Z).

## Question

Can a host alter the encrypted memo region or outgoing ciphertext in a
Sapling output while reusing the same read-only Blue review receipt?

## Result

The C23 BOLOS-shim test replays the 1,425-byte consensus spend fixture and
captures the checked branch, public facts, ZIP-243 digest, and full-wire
SHA-256 in one receipt. It then changes one byte in the note ciphertext's
memo region and, separately, one byte in the outgoing ciphertext. Both
modified transactions retain the same public spend/output counts and branch,
but each produces a different ZIP-243 digest and full-wire commitment.
The focused test passed in Release and AddressSanitizer/UBSan Debug.

This establishes byte binding for those two fixture fields through the
read-only replay. It does not decrypt either changed output, verify the
recipient or memo, establish a fee or active chain, authorize a signature,
or prove physical Blue behavior. The separate outgoing-output fixture
authenticates and opens the original ciphertext and checks its ephemeral
key and note commitment; a production signer still needs these checks and
device-derived viewing authority in one bounded approval path.
