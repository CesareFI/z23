<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Separate-account proof verifier

A push proof may reuse a compiled object only on the word of a verifier that
runs under its own account. Same-account reuse is refused, and stays refused:
anything the proving account can write, a candidate can plant. This is a
design and an uninstalled trust boundary, not a working speed path. The
current wrapper compiles cold in proof mode with `verified:no_verifier_key`.

## Fixed result.c installation packet (not installed)

The first executable scope is only
`platform/modules/base/src/result.c` on Linux x86-64 with GCC 13, using the
exact direct-source flags in `tools/verify/real_tu_probe.sh`. The local
`tools/verify/tree_closure.c` utility hashes a complete bounded tree in sorted
order, including path, entry type, mode, owner, link target and regular-file
bytes. It refuses special entries, writable entries, escaping links and
unsafe ancestry. Its output says `attest_eligible=0`: it is a prerequisite,
not an installed signer or an assertion that a compiler used only that tree.
Its local test mode permits ancestors owned by the testing UID; installed
trust requires root-owned ancestors and read-only mounts instead.

The administrator must run the following staging commands as root on a host
where UIDs/GIDs 60092 and 60093 are free. These commands create no key, socket
or running service. A pre-existing user or group with either ID is a hard
refusal; do not remap an existing account or reuse its files.

```sh
set -eu
test -z "$(getent passwd 60092)" && test -z "$(getent group 60092)"
test -z "$(getent passwd 60093)" && test -z "$(getent group 60093)"
test -z "$(getent passwd z23verify)" && test -z "$(getent passwd z23vcc)"
test -z "$(getent group z23verify)" && test -z "$(getent group z23vcc)"
groupadd --system --gid 60092 z23verify
groupadd --system --gid 60093 z23vcc
useradd --system --uid 60092 --gid 60092 --home-dir /var/lib/z23verify --shell /usr/sbin/nologin z23verify
useradd --system --uid 60093 --gid 60093 --home-dir /var/lib/z23vcc --shell /usr/sbin/nologin z23vcc
install -d -o root -g root -m 0755 /etc/z23verify /var/lib/z23verify /var/lib/z23verify/jails
install -d -o z23verify -g z23verify -m 0700 /var/lib/z23verify/key
install -d -o z23verify -g z23verify -m 0755 /var/lib/z23verify/cas /var/lib/z23verify/store
install -d -o z23vcc -g z23vcc -m 0700 /var/lib/z23vcc /var/lib/z23vcc/work
```

The later, reviewed installer must create a **root-owned, read-only mount
namespace** for the compiler account. Its root contains only the pinned GCC
driver, cc1, assembler, ELF interpreter, every loaded shared object, GCC specs
and start files actually reachable by this compile, all system include trees,
one signer-copied source snapshot at the same physical cwd and source argv
spelling as the cold build, a fixed `/dev/null`, and bounded scratch. The
developer account cannot write any mounted input. `z23vcc` cannot write any
input or see the signer key. The signer controls its source snapshot and checks
its tree before and after the child exits. The tool image and namespace
skeleton are root-owned and immutable while requests are served. The child
starts with empty environment plus an explicit, hashed whitelist, no inherited
file descriptors except stdio and authenticated IPC, no network, and fails if
any mount/confinement operation fails. Landlock remains a second layer; it
cannot replace the mount namespace because this kernel lets a Landlocked child
observe ungranted path metadata with `stat(2)` and `access(2)`.

The closure identifier must hash canonical complete tool-image and source
trees, the immutable namespace skeleton, exact environment, direct-source
argv, physical cwd, and isolation policy. Hash the trees before and after the
compile, and refuse on a changed hash or any missing/unreadable entry. The
receiver independently reconstructs the expected closure from its current
generation worktree; it must not copy the signer's claimed hash. Whole-tree
identity binds absent header lookups such as `__has_include("optional.h")`:
adding a previously absent file changes the directory hash. An observation
can become eligible only after the installed service proves this confinement
and the receiver verifies a root-pinned key and every stored artifact.

One fixed-TU acceptance run must first prove direct-source object, depfile
and stderr byte equality against a cold compile under the exact profile. RED
fixtures must replace `cc1` while leaving the GCC driver unchanged, set
`COMPILER_PATH`/`GCC_EXEC_PREFIX`, add an optional header absent in the prior
snapshot, change a DSO/spec file, inject an escaping symlink, and change an
input during compilation. Each must alter the closure or refuse before any
eligible record. The receiver must reject a wrong key, wrong argv/cwd,
tampered object/dep/stderr, and conflicting signed PASS/FAIL observations.
Until this run succeeds under the real accounts and root-owned image, proof
reuse remains cold and `attest_eligible=0`.

The proposed verifier would compile each translation unit itself, with its
own pinned compiler, and never run the candidate's build scripts. Its only
reusable product would be a signed record: "this compiler, with these flags
and complete input closure, produced these bytes". A future proof may reuse
an object only when its own input checks and the record verify under current
receiver policy.
The preprocessed stream is one input, but it is not a sufficient object key
under the current debug/LTO profiles: a real Z23 unit compiled directly from
`.c` differs bytewise from the same unit compiled from its exact `.i` stream.
`tools/verify/real_tu_probe.sh` reproduces this on Linux x86-64 with GCC
13/14 from one working directory. An eligible
verifier must compile the real source with equivalent path and working
directory semantics, then bind the implementation bytes of all tools it used.

## Threat model

1. The candidate controls its Makefile, build scripts, generators, tests,
   headers and environment. All of it runs as the developer account, with
   nothing confining it.
2. Anything that account can write is suspect: the compiler cache, warm donor
   trees, the version-control store, and the per-box proof signer key and its
   allowlist. A candidate can read that key and extend that allowlist, so
   neither can be the verifier's root of trust.
3. The attack defended here is hidden state: a planted object or PASS that
   changes a later, honest-looking proof. A malicious diff is out of scope;
   review sees it.
4. The signer never runs candidate-controlled code while it holds its key:
   no Makefiles, configure scripts, compiler plugins or spec files. The
   compiler runs under a different account with no access to that key.
5. The verifier computes its own binding hash from bytes it copied itself and
   signs with a key only its account can read. Its public key is pinned in a
   root-owned file.
6. Any doubt compiles the unit cold, and the refusal is named.

## Architecture

**Account and files.** A signer account `z23verify` owns the key and
content-addressed observations. A separate compiler account `z23vcc` runs
compiler children. The signer prepares a source snapshot that `z23vcc` can
read but cannot write. Neither account can switch to the other. The
developer account can read published items marked (r) and write none of them.

| Path | Mode | Purpose |
| --- | --- | --- |
| `/var/lib/z23verify/key/` | 0700 | Ed25519 private key |
| `/var/lib/z23verify/cas/` | 0755 | file contents copied and hashed by signer |
| `/var/lib/z23verify/store/` (r) | 0755 | content-addressed objects and signed records, write-once by protocol |
| `/var/lib/z23verify/jails/` | root 0755 | versioned jail roots; synthetic source paths are signer-writable and compiler-read-only |
| `/var/lib/z23vcc/work/` | 0700 | compiler scratch/output, no signing key |
| `/etc/z23verify/verifier.pub` (r) | root 0644 | pinned public key |
| `/etc/z23verify/toolchain.conf` (r) | root 0644 | pinned compiler identity |
| `/usr/local/libexec/z23-verifyd` | root 0755 | daemon binary |

**Request path.** A developer-facing Unix socket accepts untrusted compile
requests. A second socket between compiler and signer authenticates peers with
`SO_PEERCRED`; the signer accepts observations only from the pinned compiler
account. The signer uses `ProtectHome=yes`; the compiler runs inside a
root-owned `RootDirectory` containing only a synthetic `/home` at the exact
recorded source path. It uses `ProtectHome=no` inside that jail so the synthetic
path remains visible, while the host home is absent. A request carries the
normalized argv and recorded working directory,
the unit's relative path, a manifest of every file the preprocessor opened
(relative path to sha3), and the contents of any file missing from the store.

Request processing then:

1. the signer copies and re-hashes request bytes into its CAS;
2. the signer materializes a frozen, compiler-read-only snapshot under the
   versioned jail;
3. the compiler service checks argv against a fixed allowlist of flags;
4. a compiler child confined by Landlock (snapshot, pinned toolchain and system
   include directories, a tmp directory, no access to `key/`, no network),
   runs `-E` and requires the preprocessed-text hash to match the claim;
5. that child compiles the real source, so warnings and `-Werror` behave as in a cold
   build;
6. hands output bytes and its complete tool/input identity to the signer over
   authenticated Unix descriptors. The signer copies and hashes the output
   into its own content-addressed store before signing `zcl.verify_attest.v1`.

This sequence remains an acceptance target. The current attestation record's
`toolchain_id` must cover the driver, compiler backend, assembler, ELF loader,
dynamic libraries, GCC specs and any other executable bytes the compile used.
A driver hash alone does not qualify. Source snapshots must reproduce the
direct-source object's path semantics, including debug and LTO sections; a
matching preprocessed hash alone does not qualify.
For LTO, `recorded_cwd` means the effective physical cwd used by GCC, not a
prefix-mapped DWARF directory. Source argv spelling, symlink policy, random
seed, environment and profile are part of the expected compile input.
Current admission checks signed cwd and closure fields against receiver
expectations, and its store key includes both. No installed verifier currently
constructs those expectations. `SO_PEERCRED` attests the compiler service UID,
not the truth of data sent by a compromised compiler service. That service
remains a trust boundary requiring independent qualification.

**The record.** `zcl.verify_attest.v1` carries `toolchain_id`, `argv_norm`,
`recorded_cwd`, `pp_sha3`, `closure_sha3`, `obj_sha3`, `dep_sha3`,
`stderr_sha3` and `exit`, in one canonical little-endian encoding, followed
by the signer's public key and an Ed25519 signature over a domain-separated
message. The implementation and its byte layout live in
`tools/dev/verify_attest.h`.

**How the proof will use it.** The compiler wrapper's verified mode is set by
the proof environment and currently compiles cold. Future reuse must compute
the full expected key from the generation worktree, load an entry from the
separate verifier, and admit it under current receiver policy. Admission
checks the signature, toolchain, flags, cwd, preprocessed input, complete
closure and fetched object bytes before writing the object. The proof must
generate its own depfile for its exact target during the input check; a donor
depfile can name another `-MT` target even when object bytes match. Coverage
sidecars remain fresh. Any mismatch compiles cold; an eligible contradiction
remains blocking. The
epoch-object publish path needs the same admission.

**Admission refusals.** Each is a stable token: `no_verifier_key`,
`attest_schema_unknown`, `attest_record_malformed`,
`attest_signed_by_box_signer`, `attest_signer_not_verifier`,
`attest_signature_invalid`,
`attest_toolchain_mismatch`, `attest_argv_mismatch`, `attest_cwd_mismatch`,
`attest_pp_mismatch`, `attest_closure_mismatch`,
`attest_obj_hash_mismatch`, and `verifier_key_is_box_signer` when the pinned
key is the per-box signer key.
An exact signed record with a nonzero compiler exit returns blocking
`attest_exit_nonzero`; it is a failed compile, not a missing cache entry.

**Trust-root refusals.** The loader refuses a key file that is missing
(`no_verifier_key`, the normal state until a verifier is installed), not a
regular file, not root-owned, group- or world-writable, under any parent
directory that is not root-owned or is group- or world-writable, or
malformed. A test-only override path exists only in `ZCL_TESTING` builds.

**Why per-unit compiles, not the Makefile.** Makefiles, configure scripts and
generators are arbitrary code. Running them as the verifier would put
candidate code inside the signer's account, and their output is not a
function of anything the verifier can bind.

**Safety conditions for the proposed verifier.**

- Candidate-supplied argv is parsed against an allowlist. Source/header bytes,
  manifest and requested cwd are also untrusted inputs and require
  independent validation in the compiler service.
- Preprocessing reads files but runs no code. Includes are limited to the
  snapshot by Landlock and bounded by rlimits.
- Refused flags, because they can run other programs or write elsewhere:
  `-fplugin*`, `-specs`, `-B`, `-wrapper`, `@file`, `--sysroot`, `-iprefix`,
  `-save-temps`, `-fdump-*`, and absolute `-I`/`-include` outside the pinned
  roots.
- `GCC_EXEC_PREFIX`, `COMPILER_PATH` and `DEPENDENCIES_OUTPUT` are scrubbed.
- Generated headers are data; the preprocessed-text hash covers them.
- A record claims only what a compiler run produced, not that the code is
  correct. Candidate code must still be treated as hostile compiler input.

**Warm donors.** Same-account donor seeding stays refused. The verified path
replaces it: the verifier becomes the donor.

## Privileged installation packet (pending implementation and grant)

No installation command is valid yet: there is no daemon, signer, root-owned
toolchain closure pin, or qualified object-equivalence test in the tree. An
operator-granted install must review the exact signed source commit and binary
hashes, create the two non-switchable accounts above, pin the full compiler
closure and public key in root-owned files, and install separate Unix sockets.
The signer service needs `User=z23verify`, `ProtectHome=yes`,
`ProtectSystem=strict`, `PrivateNetwork=yes`, `NoNewPrivileges=yes`, and an
empty capability bounding set. The compiler service needs `User=z23vcc`,
`RootDirectory=` set to the reviewed jail, `ProtectHome=no` inside that jail,
the other restrictions above, no key read permission, and bounded
snapshot/toolchain filesystem access. The jail contains no host home and no
signing key. Serialize requests sharing a jail/path or use disjoint jail
versions. Qualify both directions of peer credentials, exact output
copy/hash, key isolation, cancellation, and a real debug/LTO unit before
enabling either socket. Installation and service activation need their own
operator grant; development authorization does not include them.

## Teardown

```sh
systemctl disable --now z23-verifyd.socket z23-verifyd.service z23-vccd.socket z23-vccd.service
rm /etc/systemd/system/z23-verifyd.* /etc/systemd/system/z23-vccd.*
systemctl daemon-reload
userdel z23verify
userdel z23vcc
rm -rf /var/lib/z23verify /var/lib/z23vcc /etc/z23verify /usr/local/libexec/z23-verifyd /usr/local/libexec/z23-vccd
```

Once the public key is gone, proofs go cold again automatically.

## Slices

Each slice fails closed by default.

1. **Record, pinned key, admission.**
   - 1a (landed with this document): `tools/dev/verify_attest.{h,c}` — the
     record and its canonical encoding, the store key, the root-pinned key
     loader, and the admission decision, proven by `test_verify_attest`.
   - 1b: the compiler wrapper's verified mode, which refuses everything until
     a key is pinned; the proof environment sets it, and the proof note gains
     `object_reuse_admit=unqualified(no_verifier_key)`. RED: an object
     planted in the same-account cache is served by a proof-mode compile
     today. GREEN: it compiles for real.
2. **Daemon compile core.** tools/verify/z23_verifyd.c (planned; not yet in
   the tree), using the existing
   Landlock code. Tests run it unprivileged with a trust root injected only
   under `ZCL_TESTING`. The refused flags, an out-of-snapshot include, a
   mismatched preprocessed hash and a device file are all refused; an honest
   real debug/LTO unit yields a verifiable record whose object is
   byte-identical to a local cold compile. The current experimental `.i` core
   did not pass this requirement and is not part of the admitted path.
3. **Client submission and pre-warming main.** On a miss the wrapper queues
   a bounded, non-blocking request; a `verify warm` command pre-builds main's
   units after publication. With no socket, proofs are unchanged.
4. **Units and install scripts.** `packaging/systemd/z23-verifyd.{socket,service}`
   and install/uninstall scripts, with a lint for `User=z23verify`,
   `ProtectHome=yes` and `PrivateNetwork=yes`.
5. **Receipt binding.** The compile dimension records `verified_reused` and
   the verifier's public key (receipt schema bump); the land queue refuses
   `verifier_unknown`.
6. **Deferred: test-verdict reuse.** Tests execute candidate code, so this
   needs a second, disposable account and a signer separated from the
   executor.

## Expected speed (estimates, not measured)

- Refusing warm donors alone moves the bundle phase from about 10 s to about
  205 s.
- With the verified path, each unchanged unit costs one `-E` pass (about
  30–60 ms), a signature check (about 50 µs) and a copy. For a narrow edit
  against a pre-warmed main this should recover most of the compile gap.
- Still cold: changed units, links, archives, vendor builds, generators, all
  lint gates and test groups, and any toolchain change until it is re-pinned.
- A cheaper later path could compare file hashes instead of running `-E`. It
  is sound only if it also records include lookups that found nothing.

## Open questions

1. **Binary provenance.** The daemon is built from an owner-signed commit by
   `z23verify`, and every upgrade needs root. Is that acceptable?
2. **Path equivalence.** Reproduce direct-source debug/LTO object bytes from
   the compiler account's owned snapshot without exposing the signing key.
3. **Toolchain pin.** A compiler package upgrade makes everything miss until
   someone re-pins. Manual, or a root timer?
4. **Test verdicts.** Reuse requires executing candidate code under a
   disposable account with no network. Approve in principle, or defer?
5. **Build-script bypass.** A candidate's Makefile could switch verified mode
   off. This is treated as visible in the diff. Should the proof instead
   require the wrapper for every compile?
