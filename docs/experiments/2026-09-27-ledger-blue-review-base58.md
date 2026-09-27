<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Blue Review address encoder consolidation

Local time: 2026-09-27T00:24:48-04:00

UTC: 2026-09-27T04:24:48Z

## Question

Can the uninstalled Review candidate use the same audited C23 Base58
encoder as the Wallet and host without changing displayed output addresses
or screen layout?

## Evidence

Review 0.4.4 replaces its private Base58 loop with `zcl_base58_encode`.
The host screen test still matches P2PKH and P2SH addresses against the
OpenSSL-backed host address formatter. The full Clang 22.1.6 Debug
suite with AddressSanitizer and UndefinedBehaviorSanitizer passed 19/19.
All five 320 × 480 PNG pages for the published 245-byte ZIP-243 transparent
fixture were byte-identical between the previously built 0.4.3 renderer and
the rebuilt 0.4.4 renderer. CPU: AMD Ryzen 7 PRO 8840U with Radeon 780M
Graphics.

The ARM build passed strict C23 warnings and its stack gate. The screen
path sum fell from 928 to 888 bytes with the same 2,048-byte stack reserve
and 512-byte margin. `.text` remained 32,256 bytes and `.bss` remained
6,000 bytes, leaving 144 bytes outside the reserved stack. The 0.4.4
`.text` SHA-256 is
`c7685268f58f5913196f1b9a1547484a9fe8be7f14355325d9e6745f1a3e39eb`.

Review 0.4.4 has not been installed or tested on the Blue. The code size
did not fall, and the 144-byte SRAM headroom remains too small to combine
this Review state with the receive-key state without a streaming design.
