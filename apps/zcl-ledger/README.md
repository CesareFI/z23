<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# ZCL Ledger transport experiment

This standalone C23 host communicates with Ledger Blue over Linux `hidraw`.
It reads app information, probes ZCL device apps, and installs or deletes
byte-pinned development images through the Blue's secure channel. It does not
need Ledger Live, Python, Rust, or a network connection at runtime. The host
can encode a transparent address from a supplied public key or the receive
candidate's device-derived public key. The host does not derive device keys,
sign transactions, or access recovery words.
The [Blue development guide](DEVELOPMENT.md) records the open-source C23
toolchain, image checks, and evidence gates for extending the device app.
The reusable [`zcl_tx_stream` API](include/zcl_tx_stream.h) parses unsigned,
all-transparent Sapling-v4 transaction bytes in bounded C23 state as chunks
arrive. It emits provisional input and P2PKH/P2SH output facts, then returns
aggregate facts only after the declared byte count and trailing fields pass.
This offline component is not yet connected to a Blue payment app and does not
authorize signing.
The related [`zcl_tx_stream_zip243` API](include/zcl_tx_stream_zip243.h)
computes an input-specific ZIP-243 SIGHASH_ALL digest from accepted chunks
using two independent BLAKE2b contexts. Its scriptCode, spent amount, and
consensus branch still require independent verification before a device may
use that digest to approve a payment.
The [`zcl_tx_replay_zip243` API](include/zcl_tx_replay_zip243.h) is a smaller
offline alternative: it checks three complete uploads of the same unsigned
transaction against one device-computed SHA-256 commitment and reuses one
BLAKE2b context for ZIP-243. It has not been linked into a Blue app.
The [`blue_payment_review` API](include/blue_payment_review.h) adds a bounded,
read-only output acknowledgement controller to that replay. It stops accepting
bytes after each public output until the caller acknowledges it, and rejects
chunks with even one byte beyond an output. A device app must call
acknowledgement only from a real touchscreen action. The controller is
host-tested but is not yet
connected to a Blue screen, USB payment command, or signing key.
The [`blue_payment_screen` API](include/blue_payment_screen.h) formats the
pending output into a full 35-character mainnet address and exact ZCL amount.
The host-only renderer previews a 320 × 480 light or dark screen using the
Blue SDK font bitmap. It has not been installed on the Blue.
`zcl-blue-payment-sim` runs an unsigned, all-transparent v4 transaction
through the three replay passes, simulates one acknowledgement per output,
and writes each output PNG only after the full replay validates. The branch
ID is supplied by the caller and is not checked against ZCL consensus.

## Build and test

```sh
cmake -S apps/zcl-ledger -B build/zcl-ledger -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang
cmake --build build/zcl-ledger
ctest --test-dir build/zcl-ledger --output-on-failure
build/zcl-ledger/test-blue-payment-screen /tmp/zcl-payment-light.png /tmp/zcl-payment-dark.png
build/zcl-ledger/test-blue-payment-review /tmp/zcl-unsigned-fixture.bin
build/zcl-ledger/zcl-blue-payment-sim 76b809bb /tmp/zcl-unsigned-fixture.bin /tmp/zcl-payment
```

Find accessible Ledger HID interfaces without Ledger Live:

```sh
build/zcl-ledger/zcl-ledger devices
build/zcl-ledger/zcl-ledger devices --json
```

The Blue exposes more than one HID interface. Query the app interface of an
unlocked device by its explicit path:

```sh
build/zcl-ledger/zcl-ledger app-info /dev/hidraw1
build/zcl-ledger/zcl-ledger app-info --json /dev/hidraw1
```

The HID exchange clears its response buffer and sets the returned length to
zero after a timeout, malformed sequence, or partial reply. A caller must
close and recheck the device after such a failure. Local socket-backed tests
exercise these cases; they do not substitute for a physical USB disconnect
test.

After the [ZCL Probe device app](device-blue/README.md) is installed and open,
`zcl-ledger probe --json /dev/hidrawN` checks its exact version 1 capability
reply. Version 1 reports address and signing capabilities as false. The probe
cannot succeed against BOLOS or a different app.

Exit ZCL Fixture with its touchscreen EXIT button. Z23 rejects the `quit`
command before opening USB: the Blue acknowledged a USB quit request during
a live test, then froze until a user restart. Confirm the Blue's home screen
and a BOLOS app-info reply before sending manager commands.

The separate `zcl-blue-install` executable uses OpenSSL 3's open-source C
crypto implementation for the Blue's secp256k1 and AES secure channel. It
checks the Blue's USB product ID, verifies the session's device certificate,
checks an encrypted target-ID response, and accepts only pinned image
profiles, including the unverified receive candidate. `--channel-only`
checks the secure channel without
installing. The installer targets the connected Blue v2 (`0x31010004`). See the
[device app instructions](device-blue/README.md) for the exact build and
install commands.

The optional [user controlled Blue CA](BLUE_CA.md) can sign reviewed apps
with a locally held key. It changes the device's trust configuration and is
documented separately from the unsigned diagnostic apps.
Its `zcl-blue-install /dev/hidrawN --ca-list CA_KEY_FILE` command reads the
Blue's installed app names through the authenticated channel. The dedicated
Blue returned ZCL Review in this catalog even while its icon was not visible
to the owner; the catalog alone does not establish that Review can open.

The intended everyday interface is one [ZCL Wallet device app](ROADMAP.md)
controlled by Z23. The [receive candidate](device-blue-wallet/README.md)
derives a fixed transparent key on the Blue, displays its address, and exposes
only its public key over USB. Its host command is
`zcl-ledger receive-address --json /dev/hidrawN`. The candidate is built and
simulated offline but is not installed or hardware-verified. Probe, Fixture,
Review, and Sign Test are development images with limited permissions and no
payment signing.

## Sapling transaction structure review

`zcl-tx-review` parses a raw ZCL Sapling-v4 transaction file without opening
USB. It reports transparent input and output counts, total public output
value, Sapling spend and output counts, Sprout JoinSplit count, value balance,
lock time, and expiry height. The parser requires canonical CompactSize
lengths, checks public value ranges, bounds all arrays and scripts, and
rejects trailing or truncated bytes.
It also reports the number of exact P2SH output scripts and OP_RETURN
outputs, plus whether output zero begins with an SLP token marker. These
three script facts are checked by the host only, including when `--blue` is
used. A P2SH output does not establish a multisig threshold; the redeem
script is needed. An SLP marker does not establish a valid ZSLP transfer;
token fields, input lineage, and amounts still need verification.
The CLI lists up to 32 transparent outputs with their amounts in ZCL and
zatoshi and standard P2PKH or P2SH addresses. Other scripts show their
length and SHA-256 digest without inventing a recipient. JSON sets
`output_details_truncated` when additional outputs exist. It also includes
`amount_zcl` for each listed output and `transparent_output_zcl` for the
public output total as exact eight-decimal strings alongside integer
zatoshi values. The Blue's Review app independently derives and displays
the same standard addresses, but its image has not yet been tested on the
device; neither display is payment approval.

```sh
build/zcl-ledger/zcl-tx-review --json transaction.bin
# Run the C23 app controller and all output pages without USB:
build/zcl-ledger/zcl-tx-review --json --simulate-app transaction.bin
# Save simulated 320 x 480 Ledger Blue screen PNGs without USB:
build/zcl-ledger/zcl-blue-screen-sim transaction.bin /tmp/zcl-blue-preview
# Preview larger text and dark colors, offline:
build/zcl-ledger/zcl-blue-screen-sim transaction.bin /tmp/zcl-blue-accessible --large-text --dark
# Compute the shielded signature digest with an explicit branch ID:
build/zcl-ledger/zcl-tx-review --json --branch-id 0x76b809bb transaction.bin
# After a separately reviewed ZCL Review app is installed and open:
build/zcl-ledger/zcl-tx-review --json --blue /dev/hidrawN transaction.bin
```

The JSON fields `shielded_details_verified` and `signing_ready` are always
`false`. Sapling output recipients and amounts are encrypted in the wire
transaction; this structural parser does not decrypt them or verify proofs,
signatures, ownership, fee, or consensus validity. It has no key access.
`--simulate-app` sends the transaction through the exact C23 controller
compiled into the Review app, compares its summary and optional ZIP-243
digest, and traverses every output page within the app's 2,304-byte limit.
It opens no USB device. JSON sets
`app_simulated:true` only after this check, while `blue_parsed` stays false.
The two modes are mutually exclusive. Input and review failures in `--json`
mode return an `{"ok":false,"error":"..."}` object and a nonzero exit status; error
codes distinguish invalid arguments, file access, parsing, digest, app
simulation, and hardware review failures. An app simulation does not prove
BOLOS USB, touch, EXIT, or payment authorization.
The optional `--branch-id 0xXXXXXXXX` mode computes a ZIP-243 shielded
SIGHASH_ALL digest from the full transaction with streaming personalized
BLAKE2b-256. The caller must determine the ZCL consensus branch for the
transaction's height; the command does not validate the branch ID or
consensus validity. With `--blue`, it also compares the Blue's independent
digest. Both digest values are review data, not signatures or approval.
The C23 ZIP-243 library also computes SIGHASH_ALL for a selected transparent
input when given its spent output's scriptCode and amount. Its result matched
ZIP 243's published transparent test vector. The separate C23 host preflight
binds supplied v4 previous transactions to P2PKH inputs by SHA-256d txid,
derives their amounts and scripts, checks every output type, and calculates
the fee. Its input-specific digest uses those hash-bound bytes. It does not
prove that a prevout is unspent, mature, included in the accepted chain, or
owned by the selected Ledger path. The Blue has not verified these facts and
no transparent payment signing command exists, so this digest grants no
payment authority.
The optional `--blue` mode sends at most 2,304 transaction bytes to the
[ZCL Review app](device-blue-review/README.md), verifies its review-only
identity, and requires its structural summary and transaction SHA-256 digest
to match the host's values. This checks the exact bytes received by the Blue.
`blue_parsed` is true only after that comparison succeeds. A synthetic
one-spend, one-output fixture passed this comparison on a dedicated Blue.
The app's NEXT PAGE button cycles through the structural summary and every
public output. A standard P2PKH or P2SH output page independently derives
the ZCL mainnet address from that output's script and shows its amount in
ZCL with eight decimal places. Other scripts show their byte length and a SHA-256 prefix; OP_RETURN
pages explicitly say token status is unverified. The app still cannot show
shielded recipients or authorize payments. Without `--blue`, the CLI sends
nothing over USB and `blue_parsed` is false.

The [ZCL Fixture](device-blue-fixture/README.md) tests an exact public-key
reply and host address encoding without touching the device seed. Run
`zcl-ledger fixture-address --json /dev/hidrawN` while that app is open.
Its result is explicitly marked as a fixture, never as a wallet address.

The [ZCL Sign Test](device-blue-sign-test/README.md) performs a real
seed-derived ECDSA signature over a fixed, non-transaction message after a
touchscreen tap. `zcl-blue-sign-test` verifies the signature and reports the
public ZCL transparent address. It cannot sign a payment or Sapling spend.

The path is an example. `devices --json` returns an `ok` boolean and a
`devices` array of objects with `path`, `model`, `vendor_id`, and `product_id`.
`app-info --json` returns `ok`, `name`, and `version` on success, or `ok: false`
with a stable `error` code on failure. Both commands use exit status zero for
success and nonzero for failure. App-info checks Ledger's USB vendor ID before
sending its read-only APDU. It rejects malformed responses and does not report
success merely because the USB exchange succeeded. Neither command requests
keys, addresses, or signatures.

To independently encode a ZCL mainnet transparent address from a 33-byte
compressed secp256k1 public key, run:

```sh
build/zcl-ledger/zcl-ledger address-from-pubkey \
  0279be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798
```

This example prints `t1UYsZVJkLPeMjxEtACvSxfWuNmddpWfxzs`. The command
checks the public key is on secp256k1, then computes SHA-256, RIPEMD-160, the
ZCL mainnet P2PKH prefix, and Base58Check. It uses no device or private key.
It is a validation component for device-derived public keys. It does not
query the Ledger or derive a device key.

`app-info --json` error codes are `open_failed`, `not_ledger`,
`no_app_info_response`, `device_status`, and `invalid_app_info`. Device
discovery can return `device_scan_failed`. Each Blue HID interface is listed;
the caller selects the one that answers app-info.

## Limits

The transport follows Ledger's [HID framing](https://github.com/LedgerHQ/ledgercomm/blob/master/ledgercomm/interfaces/hid_device.py)
and uses the read-only `B0 01 00 00 00` app-info command from
[Ledger's client](https://github.com/LedgerHQ/ledger-live/blob/develop/libs/ledgerjs/packages/hw-app-btc/src/getAppAndVersion.ts).
The build requires ISO C23 and contains no Rust. Ledger's
[developer guide](https://developers.ledger.com/docs/device-app/beginner/vscode-extension)
states that custom apps cannot be sideloaded onto a retail Nano X. Ledger's
[Blue-specific legacy Bitcoin app](https://github.com/LedgerHQ/app-bitcoin-legacy/tree/blue-final-release)
contains a ZClassic variant; it has not been built or installed here. The
installed ZCL Probe proves only host-device communication, app display, exit,
and secure loading. The fixture app tests a public constant, not wallet
derivation or transaction signing.
