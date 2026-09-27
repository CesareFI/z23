<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Ledger Blue transparent receive candidate

Local time: 2026-09-27T00:07:30-04:00

UTC: 2026-09-27T04:07:30Z

## Question

Can a single C23 Blue app derive and display a fixed ZCL transparent receive
address before any USB command, while a C23 host independently validates the
returned public key and reproduces the same address?

## Method and evidence

The receive candidate derives `m/44'/147'/0'/0/0` after checking Blue PIN
validation. It clears the derived private material before displaying the
address or reading a USB command. It exposes only identity and compressed
public-key APDUs. Shared C23 Base58 code formats the 26-byte address payload
on the device and host. The host checks the returned point against secp256k1
and recomputes SHA-256, RIPEMD-160, the ZCL P2PKH prefix, and Base58Check.
The published generator-key test address and a second point vector pass.

The shared APDU handler was exercised with exact requests, permission and
capacity failures, malformed headers, and 10,000 deterministic five-byte
mutations. The address-line splitter rejects early NUL and invalid Base58
characters. The host CMake project passed 18/18 tests under Clang 22.1.6
Debug with AddressSanitizer and UndefinedBehaviorSanitizer, and 18/18 under
GCC 16.1.1 Release. The CPU was an AMD Ryzen 7 PRO 8840U with Radeon 780M
Graphics. The C23 ARM build passed strict warnings and produced a 14,848-byte
`.text`, zero-byte `.data`, and 3,348-byte `.bss`. The SDK reserves 2,048
bytes of stack within the 6,144-byte app SRAM limit. Two clean builds
produced the same SHA-256 for the `.text` image:

`baf36150563cecd659692434800d5bb106a9679a9fa6e36598a3e38fb0836df2`

The C23 BAGL preview rendered 320 × 480 PNGs for the receive and error
screens. The receive preview shows all 35 address characters across three
22-pixel text lines, the fixed path, a full-address comparison instruction,
and EXIT. The preview is a layout aid; it does not establish physical display
or touchscreen behavior.

## Remaining test

The candidate has not been installed or run on the Blue. Physical install,
derivation, USB response, on-device versus host address comparison, EXIT, and
restart recovery remain unverified. No receiving funds or payment signing is
enabled by these offline results. The next test should use the pinned image
on the dedicated Blue and compare all 35 displayed characters before any
funds are sent.
