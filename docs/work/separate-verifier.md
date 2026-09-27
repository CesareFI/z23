<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Separate-account proof verifier

A push proof may reuse a compiled object only on the word of a verifier that
runs under its own account. Same-account reuse is refused, and stays refused:
anything the proving account can write, a candidate can plant.

The verifier compiles each translation unit itself, with its own pinned
compiler, and never runs the candidate's build scripts. Its only reusable
product is a signed record: "this compiler, with these flags, on this
preprocessed input, produced these bytes". The proof reuses an object only
when its own preprocessing hashes to the same value and the record verifies
under a key pinned by root.

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
4. The verifier never runs candidate-controlled code while it holds its key:
   no Makefiles, configure scripts, compiler plugins or spec files.
5. The verifier computes its own binding hash from bytes it copied itself and
   signs with a key only its account can read. Its public key is pinned in a
   root-owned file.
6. Any doubt compiles the unit cold, and the refusal is named.

## Architecture

**Account and files.** A system account `z23verify` owns everything below.
The developer account can read the items marked (r) and write none of them.

| Path | Mode | Purpose |
| --- | --- | --- |
| `/var/lib/z23verify/key/` | 0700 | Ed25519 private key |
| `/var/lib/z23verify/cas/` | 0755 | file contents by hash |
| `/var/lib/z23verify/store/` (r) | 0755 | objects and signed records |
| `/var/lib/z23verify/work/` | 0700 | throwaway source snapshots |
| `/etc/z23verify/verifier.pub` (r) | root 0644 | pinned public key |
| `/etc/z23verify/toolchain.conf` (r) | root 0644 | pinned compiler identity |
| `/usr/local/libexec/z23-verifyd` | root 0755 | daemon binary |

**Request path.** A systemd socket at `/run/z23verify/verify.sock` (owner
`z23verify`, group of the developer account, mode 0660) starts the daemon,
which runs with `ProtectHome=yes` and so cannot open a path the candidate
names. A request carries the normalized argv and recorded working directory,
the unit's relative path, a manifest of every file the preprocessor opened
(relative path to sha3), and the contents of any file missing from the store.

The daemon then:

1. stores and re-hashes the bytes itself;
2. builds a snapshot it owns;
3. checks argv against a fixed allowlist of flags;
4. in a child confined by Landlock (snapshot, pinned toolchain and system
   include directories, a tmp directory, no access to `key/`, no network),
   runs `-E` and requires the preprocessed-text hash to match the claim;
5. compiles the real source, so warnings and `-Werror` behave as in a cold
   build;
6. signs `zcl.verify_attest.v1` and publishes it under
   `store/<H(toolchain_id, argv_norm, pp_sha3)>`.

**The record.** `zcl.verify_attest.v1` carries `toolchain_id`, `argv_norm`,
`recorded_cwd`, `pp_sha3`, `closure_sha3`, `obj_sha3`, `dep_sha3`,
`stderr_sha3` and `exit`, in one canonical little-endian encoding, followed
by the signer's public key and an Ed25519 signature over a domain-separated
message. The implementation and its byte layout live in
`tools/dev/verify_attest.h`.

**How the proof uses it.** The compiler wrapper gains a verified mode, set by
the proof environment. In that mode it never reads or writes its own cache
directory. It computes its own content key from the generation worktree,
loads the stored entry, checks the signature against the pinned key, checks
that `obj_sha3` matches the bytes it read and that `toolchain_id` matches
its own compiler, and only then writes the object and depfile. Any mismatch
compiles cold. The epoch-object publish path needs the same change.

**Admission refusals.** Each is a stable token: `no_verifier_key`,
`attest_schema_unknown`, `attest_record_malformed`,
`attest_signed_by_box_signer`, `attest_signer_not_verifier`,
`attest_signature_invalid`, `attest_exit_nonzero`,
`attest_toolchain_mismatch`, `attest_argv_mismatch`, `attest_pp_mismatch`,
`attest_obj_hash_mismatch`, and `verifier_key_is_box_signer` when the pinned
key is the per-box signer key.

**Trust-root refusals.** The loader refuses a key file that is missing
(`no_verifier_key`, the normal state until a verifier is installed), not a
regular file, not root-owned, group- or world-writable, under any parent
directory that is not root-owned or is group- or world-writable, or
malformed. A test-only override path exists only in `ZCL_TESTING` builds.

**Why per-unit compiles, not the Makefile.** Makefiles, configure scripts and
generators are arbitrary code. Running them as the verifier would put
candidate code inside the signer's account, and their output is not a
function of anything the verifier can bind.

**Why this is safe.**

- The one input the candidate chooses is argv, which is data checked against
  an allowlist.
- Preprocessing reads files but runs no code. Includes are limited to the
  snapshot by Landlock and bounded by rlimits.
- Refused flags, because they can run other programs or write elsewhere:
  `-fplugin*`, `-specs`, `-B`, `-wrapper`, `@file`, `--sysroot`, `-iprefix`,
  `-save-temps`, `-fdump-*`, and absolute `-I`/`-include` outside the pinned
  roots.
- `GCC_EXEC_PREFIX`, `COMPILER_PATH` and `DEPENDENCIES_OUTPUT` are scrubbed.
- Generated headers are data; the preprocessed-text hash covers them.
- A record claims only what a compiler run produced, not that the code is
  correct, so building main's units needs no trust in main.

**Warm donors.** Same-account donor seeding stays refused. The verified path
replaces it: the verifier becomes the donor.

## Root setup (idempotent)

```sh
id z23verify || useradd --system --user-group --home-dir /var/lib/z23verify --shell /usr/sbin/nologin z23verify
install -d -o root -g root -m 0755 /etc/z23verify
install -d -o z23verify -g z23verify -m 0755 /var/lib/z23verify /var/lib/z23verify/store /var/lib/z23verify/cas
install -d -o z23verify -g z23verify -m 0700 /var/lib/z23verify/key /var/lib/z23verify/work
# binary: built by z23verify from an owner-signed commit (git verify-commit), then:
install -o root -g root -m 0755 z23-verifyd /usr/local/libexec/z23-verifyd
test -s /var/lib/z23verify/key/signer.ed25519 || sudo -u z23verify /usr/local/libexec/z23-verifyd keygen
sudo -u z23verify /usr/local/libexec/z23-verifyd pubkey | install -o root -m 0644 /dev/stdin /etc/z23verify/verifier.pub
/usr/local/libexec/z23-verifyd toolchain-id /usr/bin/gcc | install -o root -m 0644 /dev/stdin /etc/z23verify/toolchain.conf
install -m 0644 z23-verifyd.socket z23-verifyd.service /etc/systemd/system/
systemctl daemon-reload && systemctl enable --now z23-verifyd.socket
```

- Socket unit: `ListenStream=/run/z23verify/verify.sock`,
  `SocketUser=z23verify`, `SocketGroup=<developer group>`, `SocketMode=0660`.
- Service unit: `User=z23verify`, `ProtectSystem=strict`, `ProtectHome=yes`,
  `ReadWritePaths=/var/lib/z23verify`, `PrivateNetwork=yes`,
  `PrivateTmp=yes`, `NoNewPrivileges=yes`, `CapabilityBoundingSet=`,
  `RestrictAddressFamilies=AF_UNIX`, `MemoryMax=8G`, `Nice=10`.

## Teardown

```sh
systemctl disable --now z23-verifyd.socket z23-verifyd.service
rm /etc/systemd/system/z23-verifyd.*
systemctl daemon-reload
userdel z23verify
rm -rf /var/lib/z23verify /etc/z23verify /usr/local/libexec/z23-verifyd
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
   unit yields a verifiable record whose object is byte-identical to a local
   cold compile.
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
2. **One account or two.** With one account, Landlock is the only barrier
   between a compiler exploit and the key. A second account for compile
   children would be stronger.
3. **Toolchain pin.** A compiler package upgrade makes everything miss until
   someone re-pins. Manual, or a root timer?
4. **Test verdicts.** Reuse requires executing candidate code under a
   disposable account with no network. Approve in principle, or defer?
5. **Build-script bypass.** A candidate's Makefile could switch verified mode
   off. This is treated as visible in the diff. Should the proof instead
   require the wrapper for every compile?
