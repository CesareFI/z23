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
It is linked into the withdrawn Blue Wallet 0.2.1 and the uninstalled 0.2.17
review candidate. It does not authorize signing.
The related [`zcl_tx_stream_zip243` API](include/zcl_tx_stream_zip243.h)
computes an input-specific ZIP-243 SIGHASH_ALL digest from accepted chunks
using two independent BLAKE2b contexts. Its scriptCode, spent amount, and
consensus branch still require independent verification before a device may
use that digest to approve a payment.
The [`zcl_tx_replay_zip243` API](include/zcl_tx_replay_zip243.h) is a smaller
offline alternative: it checks three complete uploads of the same unsigned
transaction against one device-computed SHA-256 commitment and reuses one
BLAKE2b context for ZIP-243. The three-pass replay variant is linked into
the withdrawn Wallet 0.2.1 read-only review candidate.
The [`blue_payment_review` API](include/blue_payment_review.h) adds a bounded,
read-only output acknowledgement controller to that replay. It stops accepting
bytes after each public output until the caller acknowledges it, and rejects
chunks with even one byte beyond an output. A device app must call
acknowledgement only from a real touchscreen action. The controller is
host-tested and linked to a Blue screen and USB payment review in the
uninstalled Wallet 0.2.17 candidate. It has no signing key operation.
The [`blue_payment_screen` API](include/blue_payment_screen.h) formats the
pending output into a full 35-character mainnet address and exact ZCL amount.
The host-only renderer previews a 320 × 480 light or dark screen using the
Blue SDK font bitmap. Wallet 0.2.1 froze during a physical read-only review;
the owner restarted the Blue, and Z23 deleted all remaining ZCL apps. The
0.2.17 review screens have not been physically checked.
`zcl-blue-payment-sim` runs an unsigned, all-transparent v4 transaction
through the three replay passes, simulates one acknowledgement per output,
and writes each output PNG only after the full replay validates. The branch
ID is supplied by the caller and is not checked against ZCL consensus.
The [`blue_payment_apdu` API](include/blue_payment_apdu.h) is a host-tested,
read-only Wallet command candidate. It accepts spending replay and sequential
previous-wire verification commands, then derives each input's ZIP-243 digest
and a fee from the bound inputs and reviewed outputs. The fee page shows
the fixed account prefix and paths derived from verified previous-output
scripts. Its TOTALS page separates value sent to the two fixed Blue
addresses from value sent to other addresses, including P2SH. It rejects a previous
P2PKH output unless its hash matches one of the two Blue-derived fixed-path
hashes, and rejects branch IDs not present in Z23's mainnet consensus table.
It cannot establish which known branch is currently active. USB has no
output-acknowledgement command; the Wallet 0.2.1 candidate wires it only to a
Blue touchscreen callback. Physical payment-review behavior failed on 0.2.1;
0.2.17 is uninstalled and unverified on hardware.
The portable [signing command candidate](include/blue_payment_sign.h)
accepts exactly one input index after physical review approval, binds the
signing public key to a device-derived account hash, and returns canonical
low-S DER. Host tests verify a real secp256k1 signature and reject malformed
APDUs. Wallet 0.2.17 does not route this command or sign payments. Its
read-only DONE action cannot set the signing approval flag.

## Build and test

```sh
cmake -S apps/zcl-ledger -B build/zcl-ledger -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang
cmake --build build/zcl-ledger
ctest --test-dir build/zcl-ledger --output-on-failure
build/zcl-ledger/test-blue-payment-screen /tmp/zcl-payment-light.png /tmp/zcl-payment-dark.png
build/zcl-ledger/test-blue-payment-review /tmp/zcl-unsigned-fixture.bin
build/zcl-ledger/zcl-blue-payment-sim 76b809bb /tmp/zcl-unsigned-fixture.bin /tmp/zcl-payment
```

The local shielded-review APDU fuzzer exercises malformed commands in idle,
active, partially uploaded, and completed states. It also checks reply
bounds, reset zeroization, screen line termination, page changes, large text,
and dark mode. It uses public synthetic transaction bytes and never opens USB:

```sh
cmake -S apps/zcl-ledger -B build/zcl-ledger-fuzz \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang -DZCL_LEDGER_FUZZ=ON
cmake --build build/zcl-ledger-fuzz --target fuzz-blue-shielded-review-apdu
build/zcl-ledger-fuzz/fuzz-blue-shielded-review-apdu -runs=20000 -max_len=256
```

When `arm-none-eabi-gcc`, its `rdimon.specs`, `arm-none-eabi-objcopy`, and
`qemu-system-arm` are available, CMake builds `blue-m3-qemu` and
`blue-m0-sapling-qemu`. They execute the same C23 Sapling Fr/Fs, low-memory
Jubjub, and scalar-reduction arithmetic on QEMU's Cortex-M3 MPS2 and
Cortex-M0 micro:bit boards, each with 6 KiB of simulated RAM and a 2 KiB
stack reservation. They check a public SpendAuth key vector, a fixed public
RedJubjub signing equation with changed-response and changed-message
rejections, device-side point encoding, the exact 32-byte transaction-digest
challenge, an isolated entropy-seeded nonce derivation, a scalar-field
boundary, four scalar-reduction vectors, and a stack watermark. To select
local tools:

```sh
cmake -S apps/zcl-ledger -B build/zcl-ledger \
  -DBLUE_ARM_GCC=/absolute/path/arm-none-eabi-gcc \
  -DBLUE_QEMU=/absolute/path/qemu-system-arm
cmake --build build/zcl-ledger --target blue-m3-qemu-image blue-m0-sapling-qemu-image
ctest --test-dir build/zcl-ledger -R '^blue-(m3|m0-sapling)-qemu$' --output-on-failure -V
```

The same toolchain builds `blue-wallet-m0-qemu-image`. It runs the Wallet's
actual `main.c` on QEMU's Cortex-M0 micro:bit board with deterministic BOLOS
call stubs, checks the receive address layout, both read-only APDU replies,
an outside touch, EXIT, and a stack watermark. Run it with:

```sh
cmake --build build/zcl-ledger --target blue-wallet-m0-qemu-image
ctest --test-dir build/zcl-ledger -R '^blue-wallet-m0-qemu$' --output-on-failure -V
```

The isolated ZIP32 case also checks normal and hardened child derivation
against Z23's canonical synthetic-seed vectors. Child outputs and scratch
buffers are cleared after the emulator comparison. It signs the ZIP243 digest
of a synthetic Sapling transaction with the normal child's spending key and
checks the exact signature against the host result; host verification rejects
a changed transaction.
The shielded replay can capture a selected spend's `rk` from parsed wire
bytes and releases the verified bytes to its caller only after all six
identical uploads and the ZIP243 digest finish. This is tested with synthetic
one- and two-spend transactions; the current Blue Review app remains read only.
It can also capture one selected Sapling output's value commitment, note
commitment, ephemeral key, and outgoing ciphertext. This capture is usable
only after all six uploads match; abort or mismatch erases it. Tests cover
first and second output selection, different upload chunk sizes, and changes
in each later replay pass. The captured bytes are public wire fields, not a
verified recipient or amount. Device output decryption and note commitment
checks remain prerequisites for approving a shielded payment.
An isolated randomized SpendAuth candidate also derives
`rsk = ask + ar`, recomputes the transaction's `rk`, and signs only when it
matches. Its end-to-end public fixture runs in both ARM emulators; it is not
routed through the Wallet review or approval screens.
An isolated C23 outgoing-key derivation module checks the consensus-accepted
Sapling output against an independently computed BLAKE2b test vector. The
six-pass replay capture now supplies its public `cv`, `cm`, `epk`, and outgoing
ciphertext to a fixed-size C23 ChaCha20-Poly1305 decryptor. The decrypted
64-byte `pk_d || esk` matches Z23's independent host implementation on the
committed transaction fixture. Cortex-M0 and Cortex-M3 emulators also check
the exact output and reject an altered authentication tag. These modules
are not linked into the installed Blue app. Recipient and amount verification,
note commitment checks, device approval, and shielded signing remain absent.
An isolated Sapling note-key derivation module also matches Z23's fixed KDF
vector, independently checked with Python BLAKE2b. It requires a supplied DH
result and ephemeral public key; it does not establish their authenticity.
Neither module decrypts or authorizes a shielded payment on the Blue.

The separate [Blue seed bridge](device-blue-wallet/README.md) tests a
Ledger-specific Sapling root from a hardened BOLOS BIP32 node. It is not
linked into Wallet, and its addresses differ from standard ZIP32 roots
derived directly from a wallet seed.

This board emulates the CPU, not Ledger BOLOS, its USB interface, touchscreen,
or its app memory layout. The Wallet test uses fake BIP32 and EC calls and a
different RAM layout from the linked device app. A pass does not authorize
installing or signing with the physical Blue.
The nonce test uses public fixture bytes. The installed Wallet does not call
the nonce helper or access a Sapling device key.
The emulator reports each arithmetic case's stack watermark separately and
requires at least 512 bytes free in its reserved test stack.

To build the read-only wallet reviewer without OpenSSL or PNG installed:

```sh
cmake -S apps/zcl-ledger -B build/zcl-ledger-review \
  -DZCL_LEDGER_REVIEW_ONLY=ON -DCMAKE_C_COMPILER=clang
cmake --build build/zcl-ledger-review
```

This mode uses Z23's self-contained C23 SHA-256 and BLAKE2b implementations.
It builds `zcl-blue-wallet-review` only. The complete Ledger toolset still
requires OpenSSL 3 for secure-channel key exchange, custom-CA signing,
public-key validation, and signing-test verification.

`zcl-blue-wallet-review --test /dev/hidrawN /absolute/path/zcl-rpc UNSIGNED_TX.bin
PREVIOUS_TX.bin...` is a read-only physical review driver for an installed
compatible Wallet. Supply the path to Z23's local C23 `zcl-rpc` executable
and one complete previous transaction per input, in input order. It requires
`getblockchaininfo` to report mainnet, equal block and header heights, and
`initialblockdownload:false`, then derives
the intended next height and branch, and preflights every outpoint against
those bytes. It also queries `gettxdetail` for each input and requires an
unspent output with the same amount, script length, and SHA-256 script hash,
a confirmed height,
and coinbase maturity. It then sends the
same bytes to the Blue for independent SHA-256d, P2PKH amount, fee, and
input-specific ZIP-243 digest checks. The host derives the branch ID from
Z23's mainnet activation heights and rejects heights before Sapling. The
local node responses do not independently prove peer synchronization or
account ownership. A second tip query must match the first height and block
hash before USB access. After the Blue review, the host checks the same tip,
rechecks every input UTXO, and checks the tip once more before reporting
success. A failed tip query is reported as an unconfirmed stable tip,
without claiming that a reorganization occurred. It
stops each third-pass upload exactly at the next
output. The previous transaction bytes alone do not establish chain
inclusion or unspent status. It
cannot sign. A structurally valid Sapling-v4 transaction with shielded
spends, outputs, or JoinSplits receives an explicit unsupported-shielded
message; malformed bytes retain the generic rejection. Version 0.2.17 must
pass separate device checks before this
driver is used on the Blue again.

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
test. The timeout uses one monotonic deadline across all packet waits, so a
slow multipart reply cannot restart the timeout for each report.

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
profiles that have passed a physical open and EXIT check: Probe 0.1.0,
Fixture 0.1.0, and Sign Test 0.1.0. Wallet, Review, and Shielded Review images
remain blocked from installation. Before opening USB,
it accepts only a bounded regular image file, refuses a symlink or named
pipe, and compares the image's SHA-256 with reviewed build pins.
`--channel-only`
checks the secure channel without
installing. The installer targets the connected Blue v2 (`0x31010004`). See the
[device app instructions](device-blue/README.md) for the exact build and
install commands.

The optional [user controlled Blue CA](BLUE_CA.md) can sign reviewed apps
with a locally held key. It changes the device's trust configuration and is
documented separately from the unsigned diagnostic apps.
Its `zcl-blue-install /dev/hidrawN --ca-list CA_KEY_FILE` command reads the
Blue's installed app names and 32-byte application hashes through the
authenticated channel. A listed hash can be compared with a separately
computed installation hash; it is not the SHA-256 of the code file alone.
The read-only `--ca-verify CA_KEY_FILE app.bin` command requires a reviewed
binary and exactly one same-name catalog entry with the expected application
hash. It checks the image before opening USB and does not install or run it.
It can verify a pinned image even when installation of that image is blocked.
This command has host tests but has not yet been tried against the Blue.
The dedicated
Blue returned ZCL Review in this catalog even while its icon was not visible
to the owner; the catalog alone does not establish that Review can open.

The intended everyday interface is one [ZCL Wallet device app](ROADMAP.md)
controlled by Z23. The [Wallet 0.2.17 candidate](device-blue-wallet/README.md)
derives a fixed transparent key on the Blue, displays its address, and exposes
only its public key over USB. It also links read-only transparent output
review, previous-wire verification, digest comparison, and fee display with a touchscreen
CONTINUE action and no payment signing. The Blue also derives the internal
1/0 address and labels an exact output match without claiming change. It
requires each uploaded P2PKH previous output to match one of those two
Blue-derived hashes before returning a fee or digest. Its host
receive command is
`zcl-ledger receive-address --json /dev/hidrawN`. Wallet 0.2.1 physically
matched the receive address, then froze on a synthetic review request and was
deleted. Probe and Sign Test were also deleted; the authenticated Blue catalog
reported zero apps. Version 0.2.17 is built and simulated offline but is not
installed or hardware-verified. Probe, Fixture, Review, and Sign Test remain
development images, not payment signers.

The Wallet 0.3.7 offline candidate includes a separate final touchscreen
`SIGN ZCL` approval and ordered transparent P2PKH signing. A synthetic
fixture command prepares a previous transaction and unsigned v4 spend from
a compressed public key without needing a chain UTXO:

```sh
zcl-blue-wallet-fixture --prepare 0279be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798
```

The exact 0.3.4 image froze the Blue on opening and was deleted. The C23
installer blocks Wallet images. Version 0.3.6 passes the stricter RAM gate and
host tests but has not passed BOLOS startup on the device. Its image must pass
physical open, exit, and USB checks before this fixture is used on the Blue.
With a qualified image installed and its identity verified,
`zcl-blue-wallet-fixture --device /dev/hidrawN` can drive the same fixture
through review and request one signature after the device's `SIGN ZCL`
touch and terminal confirmation. It verifies the reply and assembles the
signed bytes in memory; it neither saves nor broadcasts them. The previous
transaction has no established chain provenance, so this command is for a
dedicated test device. The device flow has not been run on physical hardware.
The CLI checks protocol bytes, which do not authenticate the installed app
image. An authenticated manager catalog check against the pinned image is a
separate requirement before any payment use. The fixture itself cannot
establish chain ownership or spendability.
The host fixture CLI uses Z23's SHA-256 and RIPEMD-160 code and the
repository's locked libsecp256k1 archive for public-key and ECDSA
verification. It does not link OpenSSL. The archive's original source
provenance remains unresolved in its vendor manifest; its fixed digest
establishes byte identity, not a source audit. The Blue app has no OpenSSL
dependency.

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
binds supplied v1-v4 previous transactions to P2PKH inputs by SHA-256d txid,
derives their amounts and scripts, checks every output type, and calculates
the fee. Its combined API returns the fee and every input's hash-bound
ZIP-243 digest only after all prevouts and digests succeed; failure leaves
both outputs unchanged. It does not
prove that a prevout is unspent, mature, included in the accepted chain, or
owned by the selected Ledger path. The Blue cannot verify those chain facts;
its fixed-path public-hash check does not establish complete wallet ownership.
No transparent payment signing command is routed in Wallet 0.2.17, so this
digest grants no payment authority.
A separate C23 streaming selector now parses complete v1-v4 previous
transactions with 168 bytes of host parser state, checks SHA-256d against
an expected transaction ID, and returns the selected P2PKH script and amount.
Its host unit and differential fuzz tests pass. Wallet 0.2.17 links it and
calculates the fee from uploaded previous wires after full output review.
It returns a ZIP-243 digest for each input using that bound script and amount.
The resulting fee and digests are read-only facts about supplied bytes;
the Blue cannot establish that an output is unspent, while the parser alone
does not establish that it is owned.
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
