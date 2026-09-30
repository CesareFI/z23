<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Offline Shielded Review image identification

## Intent

Identify the exact read-only Blue image before any device or signing-key
operation, while retaining the installer rule that physical installation
requires a prior open and EXIT check.

## Method and result

Two isolated builds of Shielded Review 0.5.8 produced identical 33,536-byte
`.text` images with SHA-256
`6466837f121cfb3bc874adc599ff41518367734af43c658c8cfed482a8e8cd42`.
The installer profile now pins that hash with no declared signing path.

```sh
arm-none-eabi-objcopy -O binary --only-section=.text \
  apps/zcl-ledger/device-blue-shielded-review/bin/app.elf /tmp/zcl-review.bin
zcl-blue-install --image-check /tmp/zcl-review.bin
```

The Release and sanitized Debug CLIs each reported:

```text
Reviewed image: ZCL Shielded Review 0.5.8; signing path absent; installation blocked pending physical validation.
```

The `blue-install-image-cli` test also checks that an unknown regular image
and a symlink fail before USB access in offline mode. The
`blue-install-params` test keeps 0.5.8 outside the installation allowlist.
Offline identification performs no USB or key operation and does not
establish physical-device behavior. A human must inspect a new physical
open/EXIT result before changing installation authority.
