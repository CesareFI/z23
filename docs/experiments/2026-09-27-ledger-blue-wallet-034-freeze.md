<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Ledger Blue Wallet 0.3.4 physical freeze

At 2026-09-27T21:30:14-04:00 (2026-09-28T01:30:14Z), the signed
`ZCL Wallet` 0.3.4 install command for the 41,472-byte image with SHA-256
`e6c158621a68bbf30ae92a7223fe151aa9d57fd184466b0c6537c0cf39c5bf6a`
received a successful manager reply. The user reported that the Blue's home
screen froze. Linux then recorded a USB disconnect and descriptor read
timeouts (`-110`). The device re-enumerated at 21:31:20-04:00 and again at
21:31:48-04:00 after another disconnect. A catalog check immediately after
the install could not open the vanished HID interface. The successful manager
reply did not establish that the image was installed or usable.

After a normal restart, the user reported that tapping the `ZCL Wallet` icon
froze the Blue. No payment request was made and no signing result was obtained.
After another normal restart, the signed delete command for `ZCL Wallet` was
accepted. A fresh authenticated manager catalog query listed **0 apps**.
This confirms removal from the manager catalog; it does not identify the
startup failure's cause.

The exact 0.3.4 image is blocked in the C23 installer before opening USB.
Its `.bss` section is 5,476 bytes, including the 2,048-byte stack reserve,
leaving 668 bytes of the Blue's 6,144-byte app SRAM outside `.bss`. The prior
build gate required only 512 bytes of remaining SRAM. The gate now requires
1,024 bytes and rejects this image. Neither the low headroom nor USB
enumeration timeouts alone prove the freeze's cause. Host UI tests do not
emulate BOLOS startup, RAM allocation, firmware frames, or real touch and
USB events.

At 2026-09-27T21:35:13-04:00 (2026-09-28T01:35:13Z), the new installer
compiled with Clang 22.1.6 and its two focused CTest cases passed. An offline
invocation with the exact pinned 0.3.4 image and a nonexistent HID path and
CA key returned `App image is blocked after a physical device freeze` before
opening either. This is a regression gate for that image, not proof that a
future image will work on the Blue.

The next physical candidate requires an offline BOLOS startup exercise or
equivalent device-level evidence, a measured SRAM margin, and a staged
open/exit/USB interruption test before payment commands. A new image must
have a new version and pinned hash. Until those checks pass, no Wallet image
is qualified for physical payment testing.
