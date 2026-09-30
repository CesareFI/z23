<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Ledger Blue device-image rebuild at signed source e15787f50

Date: 2026-09-29T08:30:55-04:00 (2026-09-29T12:30:55Z).
Host: AMD Ryzen 7 PRO 8840U. Compilers: Clang 22.1.6, C23, and
arm-none-eabi-gcc 16.2.0.

The Wallet and Shielded Review build directories were cleaned and rebuilt
from signed source `e15787f5081e7178af28f3afb91e5dc818b260f3` against
Blue SDK `3c710b4c62ad847599a2deb0932a50dd1ae4bdff` with the reviewed
C23 patch (`git diff --no-ext-diff --binary` SHA-256
`4919fd81ba7a3a80880065aaf7898c2edd603b2586f6086a88570c73996019f6`).
Both Makefile image and stack gates passed. The code section was extracted
with `arm-none-eabi-objcopy -O binary --only-section=.text`.

| Image | `.text` bytes | `.text` SHA-256 | Intel HEX SHA-256 | `.bss` bytes | `.data` bytes |
| --- | ---: | --- | --- | ---: | ---: |
| Wallet 0.3.44 | 53,504 | `d5288c8cbc2e1e5fc2f47d6d11d14796744edf3346effd5f7eb937d65e4461a8` | `dd0b0cf0f72c1d2797941f16f198a479a6a57a63d260770f5a9f54c09e051c86` | 5,120 | 0 |
| Shielded Review 0.5.11 | 34,048 | `671d323a0d9720bdd79af3cbb53166481da04952d7101917ced5192dba1fa4ff` | `9434836d43c109bcc7d42ef7af2046f59239e34cadba7ce656d538be6b61e384` | 4,360 | 0 |

These hashes match the earlier two-copy image pins. The Wallet stack checker
reported payment upload 1,128 bytes and signing derivation 1,096 bytes on
modeled paths, with 512 bytes reserved for unmodeled frames. The Shielded
Review checker reported a largest modeled path of 888/1,536 bytes. Wallet
`.bss` reaches its 5,120-byte image-gate limit. These numbers exclude
unmodeled BOLOS frames and do not prove physical stack margin or runtime
responsiveness. Neither image was installed or opened on a Blue in this
environment.
