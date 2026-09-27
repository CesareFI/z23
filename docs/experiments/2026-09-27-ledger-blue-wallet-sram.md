<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Ledger Blue receive app SRAM budget

Local time: 2026-09-27T00:22:13-04:00

UTC: 2026-09-27T04:22:13Z

## Question

How much app SRAM remains for transparent payment review after the receive
app and reserved stack, and can the build reject a future image that consumes
the remaining space?

## Measurement

The patched Blue SDK linker grants an app SRAM region of 6,144 bytes and
reserves 2,048 bytes for the stack inside `.bss`. `arm-none-eabi-size -A`
measured the receive app's `.bss` at 3,348 bytes and `.data` at zero. The
original 748-byte headroom calculation subtracted the stack a second time.
The corrected unused SRAM after `.bss` is 2,796 bytes. The build now refuses
`.bss` above 5,632 bytes, reserving at least 512 bytes after `.bss`. Its
separate stack checker requires a 512-byte margin below the stack reserve
for named C call paths. The ARM
build passed both gates; the 14,848-byte `.text` SHA-256 remained
`baf36150563cecd659692434800d5bb106a9679a9fa6e36598a3e38fb0836df2`.
The earlier injected report of 3,585 `.bss` bytes tested the old threshold,
which was based on double subtraction. It was not a measured second image.
The corrected threshold is verified separately in the
[linker accounting experiment](2026-09-27-ledger-blue-linker-sram.md).

The Review app stores up to 2,304 raw transaction bytes, which cannot be
added to this receive app's current live state. A combined payment app
requires bounded streaming, storage reuse with verified lifetimes, or a
smaller independently justified transaction representation. Approval must
still cover every output and the fee; reducing the set of displayed outputs
would not satisfy the signing requirements. This measurement does not prove
BOLOS runtime stack usage or physical touchscreen behavior.
