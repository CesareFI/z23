<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Ledger Blue public-output pages

Recorded: 2026-09-26T20:27:07-04:00 (2026-09-27T00:27:07Z)

The prior Review screen showed only counts and a transaction hash prefix.
Version 0.4.0 adds a NEXT PAGE path through the summary and every public
output. Amounts appear in ZCL with eight decimal places. For P2PKH and P2SH,
the Blue code derives a ZCL mainnet Base58Check address from the transaction's
script hash and SHA-256 checksum. Other
scripts show their byte length and a ten-byte SHA-256 prefix. OP_RETURN
pages state that token status is unverified. Every page states that the app
is read only and cannot sign.

The screen parser reads the reviewed transaction bytes again on touch. A
new or cleared review invalidates the previous reviewed length, so page
navigation cannot use a partially received transaction. The reply cache
and screen strings reduced the maximum device review from 3,840 to 3,712
bytes. The ARM linker and initialized-data check passed: `.text` 29,952
bytes, `.data` 0, and `.bss` 6,024 bytes including reserved stack. The
extracted image SHA-256 is
`f442caa2e21e3b2f830f48f71ba23531ba6cfdf51bd4d888ee59bfd0e0e72dae`.
An offline installer check accepted that image hash before attempting to
open a deliberately nonexistent device path. Changing its first byte caused
the installer to reject the image at its hash allowlist, before USB access.

Clang 22.1.6 Debug with AddressSanitizer and UndefinedBehaviorSanitizer
passed all 12 Ledger CTest cases. GCC 16.1.1 Release passed the same 12
cases. The output-page test compares device-format addresses against the
host's address encoder for P2PKH and P2SH, checks an OP_RETURN script hash
prefix, and rejects an absent or truncated output. The repository's
cyclomatic-complexity gate passed its cap of 15. No install command, USB
request, or Blue touchscreen test was run for this image. Thus the evidence
establishes an offline build and behavior, not physical screen behavior or
payment safety.

Next test: on the dedicated Blue, install this exact pinned image, open it,
review a synthetic transaction with P2PKH, P2SH, and OP_RETURN outputs,
check each page's address and amount against the host, then exit. Any
freeze or disagreement blocks signing work until diagnosed. Shielded
recipient and amount review, trusted prevout values, fee computation,
key policy, and approval-bound signatures remain unimplemented.

Host comparison recorded: 2026-09-26T20:29:08-04:00
(2026-09-27T00:29:08Z). The host review now prints eight-decimal ZCL
amounts beside exact zatoshi values. For ZIP 243's 245-byte transparent
vector 3, both Clang Debug and GCC Release JSON output agreed: public
total `49,999,755` zatoshi (`0.49999755` ZCL), output 0 `40,000,000`
zatoshi (`0.40000000` ZCL), and output 1 `9,999,755` zatoshi
(`0.09999755` ZCL). Both local Ledger suites passed 12/12 tests. The
host output still reports `signing_ready:false` and does not establish the
spent-output provenance or payment fee.

Device observation: Review 0.4.0 installed through the owner CA and
`app-info` reported its exact name and version. A read-only ZIP-243 vector 3
review failed before reporting a result; subsequent app-info had no reply.
The screen stayed steady but EXIT did not respond. The owner restarted the
Blue to BOLOS 2.1.1 home, after which Z23 deleted Review and verified its
catalog entry was absent. No signing request was sent. The image's offline
tests missed this device failure. ARM disassembly shows the long-lived APDU
loop uses a 376-byte stack frame, the ZIP-243 digest uses 312 bytes, and
the transaction parser uses 192 bytes, within a 1,024-byte reserved stack
only before intermediate frames and callbacks. Stack exhaustion is a
plausible cause, not yet a proven diagnosis. This image is excluded from
the installer allowlist pending a measured stack fix and emulator testing.
