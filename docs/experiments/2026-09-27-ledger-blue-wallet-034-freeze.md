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

At 2026-09-27T21:39:23-04:00 (2026-09-28T01:39:23Z), the full Ledger C23
project built with Clang 22.1.6 in Debug mode and all 49 local CTest cases
passed. These tests include the installer gate and host wallet UI controller;
they still do not execute BOLOS startup on the physical device.

At 2026-09-27T21:44:06-04:00 (2026-09-28T01:44:06Z), the exact 0.3.4
source rebuilt against Blue SDK commit
`3c710b4c62ad847599a2deb0932a50dd1ae4bdff` with the reviewed patch
SHA-256 `4919fd81ba7a3a80880065aaf7898c2edd603b2586f6086a88570c73996019f6`.
The linked `.text` was 41,472 bytes, `.data` was zero, and `.bss` was 5,476
bytes. The extracted `.text` matched the previously installed image byte for
byte, with SHA-256 `e6c158621a68bbf30ae92a7223fe151aa9d57fd184466b0c6537c0cf39c5bf6a`.
The new `check-image` target rejected the build at its 5,120-byte
`.bss` limit. The largest named RAM objects were the reserved stack (2,048
bytes), payment state (1,416), USB device state (308), APDU buffer (260),
BLAKE2b state (256), UX state (176), and SHA-256 state (108). The Blue SDK
linker places `_estack` at the top of the 6 KiB SRAM region, so the 668-byte
gap above the reserved `.bss` can extend the downward-growing stack; it is
not evidence of an independent heap or firmware allowance. This measurement
identifies memory pressure but does not prove the physical freeze's cause.

The next physical candidate requires an offline BOLOS startup exercise or
equivalent device-level evidence, a measured SRAM margin, and a staged
open/exit/USB interruption test before payment commands. A new image must
have a new version and pinned hash. Until those checks pass, no Wallet image
is qualified for physical payment testing.
