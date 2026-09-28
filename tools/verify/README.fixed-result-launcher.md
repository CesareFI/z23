<!-- Copyright 2026 Rhett Creighton; SPDX-License-Identifier: Apache-2.0. -->

# Fixed result.c launcher staging

This packet prepares pinned material for one strict, non-LTO `result.c`
compile. It does not install a key, start a socket or service, run `bwrap`,
write a launch receipt, or admit reuse. `fixed_result_launcher preflight`
returns exit 2 even when every pinned byte matches. That remains true until
the root launcher, worker ACK, signer, publisher, and receiver are qualified
under distinct host UIDs. Same-UID fixtures are always ineligible.

## Installed policy and pins

The administrator stages reviewed files in root-owned mode 0700
`/root/z23verify-staging`. The install script requires already-created exact
accounts `z23verify` UID/GID 60092 and `z23vcc` UID/GID 60093, plus root-owned
source, tool, and check images at the paths in
`fixed_result_launch_policy.v1`. It does not create accounts or images.

The staged policy's SHA3-256 is
`1315fa7fb2a718829a881955bf7415c3b4bdabc30d89ddeda0e0781983cc4abe`.
The strict argument profile's SHA3-256 is
`5fb3b13597488c20a9f5aeca2b654fad92b39714d93069c12c082206977aadaf`.
`env_sha3` is SHA3-256 of the literal ASCII bytes ending in LF:

```text
z23verify.fixed_result.env.v1
LC_ALL=C
TZ=UTC
TMPDIR=/tmp
PATH=/usr/bin:/bin
```

Its current digest is
`19c5ed02759b18a210013d167d7277a014dde893e1b8edaab69abc029700a3ec`.
The policy names the root image paths, fixed host UIDs, `/zclassic23`,
read-only tool/source mounts, private `/work` and `/tmp`, no network, and the
exact seccomp-filter path. A flag saying seccomp mode 2 or a read-only mount
flag alone does not establish this policy. An installed launcher must prove
the actual namespace/filter and the host-side UID before issuing a receipt.

`/etc/z23verify/fixed_result.pins` is one root-owned nlink-1 regular file
with mode 0444 and root-owned, nonwritable ancestors. Its exact LF-only field
order is:

```text
z23verify.fixed_result.pins.v1
strict_args_sha3=<64 lowercase hex>
source_image_sha3=<64 lowercase hex>
tool_image_sha3=<64 lowercase hex>
worker_sha3=<64 lowercase hex>
launcher_sha3=<64 lowercase hex>
check_image_sha3=<64 lowercase hex>
env_sha3=<64 lowercase hex>
policy_sha3=<64 lowercase hex>
seccomp_filter_sha3=<64 lowercase hex>
bwrap_sha3=<64 lowercase hex>
tree_closure_sha3=<64 lowercase hex>
```

The three image roots are the installed, UID-bound `tree_sha3` values printed by the pinned
`z23-tree-closure hash <absolute-image-path> 0` helper, not a list of only
opened headers. The tree hash includes path names, directory entries,
ownership, modes, and file bytes; absent optional headers therefore remain
bound. The launcher rehashes the helper before invoking it and rehashes all
three images during preflight. The administrator verifies these values and
the reviewed executable bytes before staging pins. A writable source
checkout, an unreviewed binary, or a pin supplied by the signer is not a
root of trust.
The receiver's portable strict-v2 source identity uses `content_sha3`
and independently rechecks current source, headers, search namespace, and
preprocessed bytes. It must not equate a developer-owned checkout's UID-bound
tree hash to the root-owned installed image hash. `check_image_sha3` must
include an independently pinned current-main proof executor and publisher;
a candidate-built `zcc` cannot establish admission authority.

After staging, the administrator can run
`platform/deploy/fixed-result-launcher-install.sh install`. It checks file
ownership, the two fixed profile/policy digests, account IDs, image presence,
and refuses any previously installed target before copying anything. It then copies
root-owned binaries/configuration and runs the installed preflight. The only
successful installer output is
`fixed_result_installed_material=1 service_started=0 signed_observation=0 attest_eligible=0`.
An absent or changed pin, image, filter, worker, launcher, bwrap, or tree
checker refuses by name; there is no fallback to same-UID signing.

## Launch receipt contract for the next slice

The root launcher alone may create
`/var/lib/z23verify/launches/<32-lowercase-hex-id>/` (root:root 0700).
After a normal worker exit and a live host-UID/mount witness, it copies all
four worker FDs into root-owned, nlink-1, mode-0400 siblings named `object.o`,
`deps.d`, `stderr.bin`, and `preprocessed.i`. It writes `launch.v1` last as a
root-owned, nlink-1, mode-0444 file. The receipt is fixed-order ASCII with
terminal LF:

```text
z23verify.launch.v1
launch_id=<32 lowercase hex>
request_nonce=<32 lowercase hex>
recorded_cwd=/zclassic23
source=platform/modules/base/src/result.c
target=<exact build/test-rel-obj/epochs/<64 hex>/platform/modules/base/src/result.o>
strict_args_sha3=<64 lowercase hex>
source_image_sha3=<64 lowercase hex>
tool_image_sha3=<64 lowercase hex>
worker_sha3=<64 lowercase hex>
launcher_sha3=<64 lowercase hex>
bwrap_sha3=<64 lowercase hex>
check_image_sha3=<64 lowercase hex>
env_sha3=<64 lowercase hex>
policy_sha3=<64 lowercase hex>
mount_namespace_dev=<decimal>
mount_namespace_ino=<decimal>
compiler_ruid=60093
compiler_euid=60093
compiler_suid=60093
compiler_rgid=60093
compiler_egid=60093
compiler_sgid=60093
supplementary_groups=0
cap_effective=0
cap_permitted=0
cap_inheritable=0
cap_ambient=0
no_new_privs=1
seccomp_mode=2
seccomp_filter_sha3=<64 lowercase hex>
worker_exit=0
object_size=<decimal>
object_sha3=<64 lowercase hex>
dep_size=<decimal>
dep_sha3=<64 lowercase hex>
stderr_size=<decimal>
stderr_sha3=<64 lowercase hex>
pp_size=<decimal>
pp_sha3=<64 lowercase hex>
```

The launcher sends the signer a root-authenticated one-packet statement with
launch ID and receipt SHA3 plus descriptor copies of the root-owned receipt
and four artifacts. The signer copies those bytes into private staging; the
offline root publisher independently descriptor-opens the private root launch
record and compares its exact bytes and pins before publishing. A launch
receipt is evidence of this one execution only. The receiver still derives
the expected closure from its own current inputs and policy and verifies the
signed record and artifact bytes. An unqualified or missing launch receipt
is a cold/refused proof, never a HIT.
