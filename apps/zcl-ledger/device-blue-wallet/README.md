<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# ZCL Wallet candidate for Ledger Blue

Version 0.3.7 enables the pinned Blue SDK's stack canary and requires that
flag at compile time. Two clean builds from independent pinned SDK trees
produced the same 41,216-byte `.text` image, SHA-256
`64839dd399415af205fcf0c02a9ca4f5283c45d0e6103df3747759847c4bc0fd`.
The [canary experiment](../../../docs/experiments/2026-09-27-ledger-blue-wallet-037-canary.md)
records the linked instruction check and its limits. This image is not
admitted by the installer and has not run on a physical Blue.

The isolated `candidate/blue_zip32_seed_device.c` adapter checks PIN state
and requests a hardened BOLOS BIP32 node for a Ledger-specific Sapling root.
It is compiled with the pinned Blue SDK and tested with a host syscall shim,
but is not linked into the Wallet image. No Sapling key or signing APDU is
available in Wallet 0.3.6. This mapping is not the standard ZIP32 root of a
wallet seed; recovery software would need the same documented mapping.

Version 0.3.6 runs the real app startup and EXIT controller in a C23 host
SDK shell. The shell checks the receive address, derivation failure screens,
secret-workspace wiping, USB reset and suspend, approval timeout, and EXIT
event. The EXIT test now routes a synthetic finger-release coordinate through
the shown touchable element bounds and checks that a touch outside the button
does nothing. It scripts valid and malformed APDUs through the app main loop and
injects receive, send, and post-reply display exceptions. It enforces one
reply per request and injects USB reset and suspend during a scripted visible
payment reply. The payment route uses a stub; this shell does not exercise
signing. It found that a failed internal derivation left formatted address
lines in RAM; the app now clears them before showing the error screen.
Two separately patched SDK builds produced identical 40,960-byte `.text`
images, SHA-256
`435b6f03a62e99daa668b85c71e895f7b64ad7a056b575367c5bf2af460e09c3`.
The host shell does not run BOLOS or physical USB and touch. This image has
not been installed and is not admitted by the installer.
The [startup experiment](../../../docs/experiments/2026-09-27-ledger-blue-wallet-036-startup.md)
records the test coverage and limits.
The [stack-path audit](../../../docs/experiments/2026-09-27-ledger-blue-wallet-036-stack-audit.md)
corrects the named-frame gate to include `main` on payment and signing calls.
The largest measured named path is 1,056 bytes; BOLOS frames remain excluded.
The [Cortex-M3 startup test](../../../docs/experiments/2026-09-27-ledger-blue-wallet-036-m3-startup.md)
runs `main.c`, both receive APDUs, and EXIT with deterministic firmware-call
stubs. Its stack watermark is 976 bytes in the tested path; it does not run
BOLOS or the linked Wallet image.
The [Cortex-M0 EXIT packet test](../../../docs/experiments/2026-09-27-ledger-blue-wallet-m0-touch.md)
checks an outside touch and a center-button release while the ARM harness
waits for another APDU. It also checks that USB reset and suspend abort two
stubbed payment displays and restore the receive screen after their replies.
Its 992-byte stack watermark applies only to those test routes; the payment
parser and signer are stubbed.

Version 0.3.5 shares RAM between mutually exclusive boot derivation and
payment state, and between output text and previous-transaction parsing.
It keeps the 16-input limit and requires 1,024 bytes of SRAM above `.bss`.
Two separately patched Blue SDK builds produced identical 40,960-byte
`.text` images with SHA-256
`d99812f8ed00e4accc3466efe1eb400b756a38677a84175897785a031e2db448`.
The app has not been installed or opened on a physical Blue. Version 0.3.4
froze on opening and was deleted; its exact image is blocked by the installer.

Version 0.3.4 checks the device-derived public key against the selected
account's startup-derived HASH160 before ECDSA runs. A missing account
binding, failed hash, or mismatched key clears the reply without signing.
Version 0.3.3 cleared a rejected payment request before returning to the
receive screen after the APDU reply. Earlier candidates could abort the
review but still mark the payment view visible.

Version 0.3.0 routes the one-byte-index INS `29` signing command after a
separate final touchscreen `SIGN ZCL` tap. The Blue displays the amount to
other addresses, fee, derivation path, and explicit `CHAIN UNCHECKED` and
`BRANCH UNCHECKED` warnings before that tap. A `NO SIGN` tap completes the
read-only review without arming signing. The approved latch is single-use per
input, ordered, and bound to the Blue-derived ZIP-243 digest and its verified
previous output. The command returns the index, path, compressed public key,
and canonical low-S DER signature. The host must verify the signature and
assemble the transparent transaction separately. The app never broadcasts.
Version 0.3.0 has passed SDK-shim touchscreen tests and two independent SDK
builds. It has not been installed or tested on a physical Blue. Its protocol
identity is version `0C`, capabilities `1F`; version `0B`/`0F` remains the
read-only identity. Sapling spends, shielded multisig, ZSLP, and P2SH
redemption are not supported by this candidate. Do not use it with funds.
The existing host review command accepts both protocol identities and never
sends INS `29`; on version 0.3.0 the owner chooses `NO SIGN` to finish that
read-only workflow.

## Read-only predecessor

Version 0.2.18 derives
`m/44'/147'/0'/0/0` on the Blue after PIN validation, retains only the
compressed public key, and displays its ZCL mainnet P2PKH address across
three large-text lines. The host reads the public key through INS `02`,
validates that point on secp256k1, and independently computes the address.
The owner must compare all 35 characters with the Blue display before using
the address. The app also derives the public hash for
`m/44'/147'/0'/1/0` and identifies an exact output match as `OWN INTERNAL
1/0`. The output label uses a larger 22-pixel font. It does not call that
output change because chain state and account policy are not proven. EXIT
returns to the home screen.

The same app now has read-only transaction review commands. It accepts an
unsigned, all-transparent v4 transaction in three complete passes, pauses
at each P2PKH or P2SH output, displays the exact amount and all 35 address
characters, and requires a touchscreen CONTINUE tap before the next chunk.
EXIT cancels and returns home. After the three-pass review, Z23 uploads each
complete previous transaction in input order. The app checks SHA-256d against
its captured outpoint, selects the indexed P2PKH output, requires its HASH160
to equal one of the two Blue-derived public-key hashes, derives all input
amounts, and displays the fee calculated from those inputs and the reviewed
outputs. For each bound input it also returns a device-computed ZIP-243
SIGHASH_ALL digest using that previous output's exact script and amount, the
reviewed spending wire, and the supplied branch ID. Z23 compares each digest
with its independent host calculation. The Blue rejects a branch ID absent
from Z23's mainnet consensus table, but cannot verify which known branch is
active at the current height, nor chain inclusion, UTXO status, or maturity.
The fixed-path hash match does not establish a complete wallet ownership policy.
There is no USB output acknowledgement, payment signature,
private-key export, path selection, Sapling spend, multisig, or token command.
The portable C23 [signing-boundary experiment](../../../docs/experiments/2026-09-27-ledger-blue-signing-boundary.md)
compiles against the Blue SDK and passes host signature tests, but the
read-only app does not call it or expose a signing APDU.
The fee page shows the Blue-derived account prefix `m/44'/147'/0'`, the
verified input path or both paths, `CHAIN UNCHECKED`, `BRANCH UNCHECKED`,
and `NO SIGNING`. Its TOTALS button shows device-derived output value to
other addresses, value to the two fixed Blue addresses, and the fee. P2SH
outputs remain in the other-address total even if their script hash equals
a public-key hash. BACK returns to the fee page. These totals appear only
after full transaction replay and input binding. Version 0.2.1 reached the
Blue: its receive
screen and EXIT worked, and all 35 address characters matched the host result
`t1RAmKL4KFauUXGswvMvk66aS5UL33ck1Uz`. A synthetic transaction review
then stopped USB replies and left EXIT unresponsive. The owner restarted the
Blue; Z23 deleted Wallet, Sign Test, and Probe and verified an empty catalog.
The 0.2.0 and 0.2.1 review images are excluded from the installer. Version
0.2.2 deferred screen redraw until after the APDU reply. Version 0.2.3 also
keeps the transmit and redraw outside the request exception handler, so a
post-reply display exception cannot schedule a second response. A receive
exception before a complete APDU now unwinds to the outer app handler.
Version 0.2.4 labels an exact P2PKH hash match to the
Blue-derived fixed account as “THIS ACCOUNT,” other P2PKH outputs as “OTHER
ADDRESS,” and P2SH outputs as “P2SH ADDRESS.” It does not infer ownership of
P2SH or call an output change without verified inputs and account context.
Version 0.2.12 enlarged the fee and totals labels and touch-control text.
The host UI test compiles the actual wallet screen code with SDK shims,
checks that its labels fit the Blue viewport, follows TOTALS, BACK, and EXIT,
and verifies that invalid output accounting ends review. Version 0.2.13
adds CONFIRM on the totals page and a read-only REVIEW CONFIRMED screen. The
tap arms a one-use, ordered digest latch after every previous output and the
fee have been verified. Verified digests reuse the completed outpoint slots,
so the input limit remains 16 without increasing `.bss`. No APDU can consume
the confirmation latch or request a signature. INS `28` still returns each
computed digest before confirmation for host comparison. Version 0.2.13
remains uninstalled. Version 0.2.14 compiles a fixed-path SDK signing
callback and wipes its private state during command, USB, and exit cleanup.
The callback has no reachable APDU, is removed from the linked image, and
has not signed on this Blue. A host SDK shim tests both fixed paths, locked
PIN rejection, malformed SDK results, zeroed replies, and private-state
erasure.
Version 0.2.15 separates read-only REVIEW CONFIRMED from signing approval.
The Blue's CONFIRM button cannot arm the digest latch or authorize the
unrouted signing command. A separate signing approval flow would be required
before any payment signer could be enabled.
Version 0.2.16 labels the read-only totals action DONE, labels its final
page REVIEW COMPLETE, and displays the unchecked chain and branch warnings
on separate 22-pixel lines. The simulator verifies that every label fits
the Blue viewport. This image has not been installed on a physical Blue.
Version 0.2.17 ignores duplicate DONE taps and delayed BACK or TOTALS taps
after the review completes. The SDK-shim touchscreen test confirms that the
completion page remains visible and no signing approval is set. This image
has not been installed on a physical Blue.
Version 0.2.18 also ignores repeated CONTINUE taps after an output
has been acknowledged and delayed CONTINUE taps after EXIT. The SDK-shim
test checks the output address lines, account label, value, acknowledgement
count, and resulting screen. This revision has not been installed on a
physical Blue.
Its output screen source and standalone preview produce identical 320 × 480
RGB pixels in the SDK shim for a device-formatted account output; this does
not establish physical framebuffer identity.
Do not receive funds or sign payments with it.

## Build

Use Ledger's open-source Blue SDK at revision
`3c710b4c62ad847599a2deb0932a50dd1ae4bdff` with the reviewed
[`C23 SDK patch`](../toolchain/blue-secure-sdk-2.1-c23.patch). Apply that
patch to a clean SDK checkout. Use Clang 22.1.6 and ARM GCC 16.2.0:

```sh
git -C /path/to/blue-secure-sdk checkout 3c710b4c62ad847599a2deb0932a50dd1ae4bdff
git -C /path/to/blue-secure-sdk apply /path/to/z23/apps/zcl-ledger/toolchain/blue-secure-sdk-2.1-c23.patch
make -C apps/zcl-ledger/device-blue-wallet \
  BOLOS_SDK=/path/to/blue-sdk \
  ARM_INCLUDE_DIR=/path/to/arm-none-eabi/include \
  GCCPATH=/path/to/toolchain/bin/ \
  CLANGPATH=/path/to/clang/bin/
arm-none-eabi-objcopy -O binary --only-section=.text \
  apps/zcl-ledger/device-blue-wallet/bin/app.elf /tmp/zcl-wallet.bin
sha256sum /tmp/zcl-wallet.bin
```

The build rejects initialized `.data`, keeps at least 1,024 bytes of app SRAM
after `.bss`, and checks named derivation, upload, formatting, replay, and
touch paths against the 2,048-byte stack reservation with a separate
512-byte margin. Before loading SDK make definitions, even for `clean`, it
checks the pinned SDK revision, exact reviewed patch diff, and absence of
staged changes or untracked SDK files. The linked
0.2.14 image has 33,792 bytes of `.text`, 5,472
bytes of `.bss`, and zero `.data`. Its `.bss` includes the linker-reserved
stack; 672 bytes remain after that section in the 6,144-byte app SRAM
region. The largest named C path sums to 752 bytes, excluding BOLOS firmware
frames. Two independent builds using patched SDK trees produced `.text`
SHA-256 `067744e45fbad645850dd7a8cf8cdfb1f1b4b8ede585d4c57c61fa5f962788b7`.
Version 0.2.15 produced 33,792 bytes of `.text`, 5,472 bytes of `.bss`,
zero `.data`, and identical `.text` SHA-256
`7cefe528eeee5407edd40306951bb604f467ef526619828f246a2fe519ebf3e6`
in two independently patched SDK trees. Its largest named C stack path
remains 752 bytes, excluding BOLOS frames.
Version 0.2.16 produced 33,792 bytes of `.text`, 5,472 bytes of `.bss`,
zero `.data`, and identical `.text` SHA-256
`2c6000584ccd6826c5ea92133bad0ab0dd3926afbc3015c9f8ab868a77f38fb6`
in two independently patched SDK trees. Its largest named C stack path
remains 752 bytes, excluding BOLOS frames.
Version 0.2.17 produced 34,048 bytes of `.text`, 5,472 bytes of `.bss`,
zero `.data`, and `.text` SHA-256
`386c39a9861431501e57c22fbb312a087f7623362a60800b6b602e229ff288bd`
in two independent patched SDK builds. Its largest named C stack path remains
752 bytes, excluding BOLOS frames.
Version 0.2.18 produced 34,048 bytes of `.text`, 5,472 bytes of `.bss`,
zero `.data`, and `.text` SHA-256
`a2b78a3add93ca47177e2307c5c2ef50348240c2a8aff5d0bac37686425afd7a`
in builds against two independent patched SDK trees. Its largest named C
stack path remains 752 bytes, excluding BOLOS frames.
The stack gate also checks four currently unreachable signing paths through
the strict command parser. Their largest named C path is 728 bytes; the gate
rejected a deliberate 1,600-byte signer-frame substitution. A separate
forced-link experiment included the signing parser, boundary, and SDK
callback without adding a routed APDU. It used 37,376
bytes of `.text` and 5,472 bytes of `.bss`, with identical `.text` across
two independent SDK builds. These figures do not measure BOLOS firmware
frames or physical signing behavior.
The [signing callback experiment](../../../docs/experiments/2026-09-27-ledger-blue-signing-callback.md)
records the tests and limits.
The [signing footprint experiment](../../../docs/experiments/2026-09-27-ledger-blue-signing-footprint.md)
records the forced-link and stack-gate results.
The installer previously accepted the independently reproduced 0.3.4
`.text` image with SHA-256
`e6c158621a68bbf30ae92a7223fe151aa9d57fd184466b0c6537c0cf39c5bf6a`.
That image is now blocked after its physical startup freeze.
Version 0.3.1 resets the payment view when a new review begins, so an
earlier signing page cannot remain selected for the new transaction.
Version 0.3.2 expires an unconsumed final touchscreen approval after 30
seconds on the SDK ticker. USB reset and suspend also abort the review.
Device-side USB, screen, EXIT, and recovery checks are pending.
The host-tested candidate INS `29` requires exactly one input-index byte and
returns one verified-path public key and normalized ECDSA signature only
after touchscreen approval. Wallet 0.2.14 does not route this command.
The separate C23 host reply verifier checks exact response framing, expected
input index and path, public-key HASH160, canonical low-S DER, and a
caller-verified signature over the device-derived ZIP-243 digest before any
signature bytes can enter transaction assembly. Wallet 0.2.18 still does not
route INS `29`.
The C23 host assembler can place a verified P2PKH reply into each empty
transparent input script, preserving the reviewed outputs and requiring the
expected input index and ZIP-243 digest for every signature. This assembly
path is host-tested only. A C23 host collector now requires the version 12
signing identity, waits for a final approval callback, requests INS `29` in
input order, verifies each returned signature, and clears every collected
signature on failure. The fixture-only CLI calls this collector after final
touchscreen approval and assembles the result in memory. It has no save or
broadcast path and has not been run on the physical Blue.

## USB protocol

All APDUs use CLA `A5`, P1/P2 zero, and an exact one-byte `Lc`.

| INS | Reply before `9000` |
| --- | --- |
| `01` | `ZCL`, protocol version `0C`, receive, review, previous-wire, digest, and signing-candidate capability `1F` |
| `02` | 33-byte compressed public key when the address is ready |
| `20` | Begin read-only replay: 12-byte length, input index, known mainnet branch ID; unknown IDs fail closed |
| `21` | Feed one chunk; reply reports pass and pending output |
| `22` | Advance replay pass; reply reports pass and output count |
| `23` | Finish complete replay; reply reports output count |
| `24` | Cancel review |
| `25` | Query six nonsecret review-state bytes |
| `26` | Begin the next previous wire with a four-byte little-endian length |
| `27` | Feed previous-wire bytes; exact SHA-256d and structure are checked at finish |
| `28` | Finish the previous wire only if its P2PKH hash equals a Blue-derived external or internal hash; reply contains bound count, input count, fee-ready flag, eight-byte fee, and 32-byte input ZIP-243 digest |
| `29` | Sign the next ordered input only after the final `SIGN ZCL` touch; reply contains input index, path, compressed public key, DER length, and low-S DER signature |

INS `02` returns `6985` if derivation or address formatting fails. A review
upload chunk must stop on the exact output boundary. Only the touchscreen
CONTINUE callback acknowledges that output; USB cannot do so. Previous-wire
commands are accepted only after all outputs and the complete spending wire
have been reviewed. Any malformed command invalidates the review. USB reset
or suspend also cancels an idle review and returns to the receive screen. No
read-only command signs or approves a payment. INS `29` requires separate
touchscreen signing approval in version 0.3.0.
After hardware validation, run
`zcl-ledger receive-address --json /dev/hidrawN` while the app is open and
compare the returned address with all characters on the Blue screen.
