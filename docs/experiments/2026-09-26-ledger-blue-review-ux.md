<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Ledger Blue Review offline screen-flow check

Recorded: 2026-09-26T21:44:59-04:00 / 2026-09-27T01:44:59+00:00.

## User-facing change

The initial screen now tells the owner to tap NEXT PAGE after sending a
transaction. The summary names the public output total as `OUTPUTS`, says
`FEE UNKNOWN`, and states that shielded details are hidden and signing is
unavailable. The wording avoids treating structural review as payment
approval.

## Evidence

The C23 simulator uses the controller source compiled into the Blue image.
It completed the published transparent fixture, displayed its two P2PKH
outputs, returned to the summary, and cleared back to the waiting screen.
A second synthetic transaction displayed a P2SH address and an OP_RETURN
script page marked `TOKEN STATUS UNVERIFIED`. The simulator also exercised
10,000 deterministic malformed APDUs with fixed seed `0x2345abcd` and
checked state bounds and screen termination after each command.

Clang 22.1.6 Debug with AddressSanitizer and UndefinedBehaviorSanitizer:
13/13 local CTest cases passed. GCC 16.1.1 Release: 13/13 passed. The
Ledger Blue ARM build passed with a 2,048-byte stack reserve; measured
APDU and screen paths were 768 and 928 bytes, respectively, plus a required
512-byte margin. `.text` was 29,696 bytes, `.data` zero, and `.bss` 6,024
bytes. Extracted `.text` SHA-256:
`1df563d4ef89cedaa7be4513c8697e729b1c7a8cbca8fb3261cb5203a8f0a88e`.

## Remaining evidence

The simulator does not emulate BOLOS USB, BAGL rendering, touch input, or
the EXIT callback. Review 0.4.2 was not installed or added to the C23
installer's image allowlist. No signing or fee verification was added.
