<!-- Copyright 2026 Rhett Creighton; SPDX-License-Identifier: Apache-2.0 -->

# Test-fast result.c key v2 (format only)

`fixed_result_key_v2.c` formats the expected input identity for one existing
`TEST_FAST` GCC 14 compile reached by `dev-proof-bundle-prefork`. It does not
load pins, authenticate an image, verify a mount, establish account separation,
or set `attest_eligible`. No
caller may use successful formatting as permission to reuse an object.

The fixed profile is the exact 183 LF-terminated ordered arguments in
`fixed_result_fast.args`, SHA3-256
`5e8a1cafce7350ff3c335c6a714f59c75c1e646de82eb03d076f68bdad244e1c`.
The first argument is `cc`; the final profile argument is the literal
`-frandom-seed=platform/modules/base/src/result.c`. Only a target shaped
`build/test-obj/epochs/<64 lowercase hex>/platform/modules/base/src/result.o`
is accepted. The constructor requires the *actual* ordered 193-token driver
argv: 183 profile tokens (starting at `cc`), then
`-MMD -MP -MF <dep> -MT <target> -c -o <object> source`.
It checks every fixed token, expands
`@CWD@` only against the caller's measured physical cwd, and requires the
target token to match the supplied epoch target. The constructor also requires
the complete actual `execve` environment in this exact four-entry order:
`LC_ALL=C`, `TZ=UTC`, `TMPDIR=/tmp`, `PATH=/usr/bin:/bin`. The caller must
prove the epoch was derived by trusted current-main Make rules, the compiler
image and actual environment were observed, and the two destinations lie in
private safe scratch. The formatter accepts only matching worker paths
`/work/result.<6 alphanumeric>/{deps.d,result.o}` or matching Linux ZCC
paths `/proc/self/fd/<decimal>/{result.d,result.o}`. The caller must verify
the actual directory descriptor or worker scratch ownership and identity.
This pure formatter cannot observe `execve` or prove
those external facts. An untrusted caller can fabricate matching parameters.

`argv_norm` is the bytes `z23verify.fixed_result.argv.v2\n`, followed by each
profile line as decimal byte length, colon, and raw bytes, then the same
encoding of `-MMD`, `-MP`, `-MF`, `<DEP>`, `-MT`, `<EPOCH_TARGET>`, `-c`, `-o`,
`<OBJECT>`, and `platform/modules/base/src/result.c`. No separator or NUL is
inserted. Only the three destination/target values are normalized. The
`recorded_cwd` is `/zclassic23`, which must be the worker's actual physical cwd
inside its qualified jail. The receiver's changing physical proof cwd belongs
in the local execution receipt, after fresh `-E` and a cross-cwd byte-equality
check. The LTO build-only profile cannot use this key.

`closure_sha3` is SHA3-256 of a length-prefixed domain text
`z23verify.fixed_result.closure.v2`, then **twelve** raw 32-byte roots in
order, then length-prefixed `/zclassic23`, source spelling, and complete
`argv_norm`. The first is the portable source-content tree. The next eleven
match root-owned `fixed_result.pins.v1` exactly:

```text
profile_args  source_image  tool_image  worker  launcher  check_image
environment   policy        seccomp     bwrap   tree_checker
```

`profile_args` maps the launcher's `fast_args_sha3` field, whose value must
pin the test-fast profile in this version. `source_image` is the
installed UID-bound tree, separate from the portable source-content root.

The check image must contain the signer, publisher, receiver admission code,
key constructor, pinned current-main proof executor, and all check logic.
The root-owned installer and receiver must prove those membership and image
bytes. The signer key and box key are independently reloaded, pinned, and
checked by the admission policy; neither is treated as a substitute for a
compiler input root. Revocation must refuse any old observation.

The length prefix is one unsigned 64-bit little-endian byte count. The
environment root must equal SHA3-256 of these exact bytes:

```text
z23verify.fixed_result.env.v1
LC_ALL=C
TZ=UTC
TMPDIR=/tmp
PATH=/usr/bin:/bin
```

The final LF is included. `toolchain_id` is
`z23.gcc14.fast_result.v2:<lowercase tool-image root>`.
`pp_sha3` is the SHA3-256 of the receiver's fresh checker `-E` stream;
the existing `zcl_verify_attest_store_key` then binds the five expected fields.
The source-content root measured from the current developer tree must match
the portable root pin. It must **not** be compared to the installed UID-bound
tree hash. All twelve roots currently enter the formatter as caller data;
the root-owned pin loader, live mount/image checks, and source namespace proof
are still required before either signer or receiver can call them trusted.
Until every ordered project search root (including absent earlier headers),
system include tree, compiler backend, loader, DSO, specs and exact installed
mount generation are qualified, the named outcome is
`include_search_namespace_unqualified`; no eligible expected key exists.

The signed donor `deps.d` remains donor evidence. It cannot be copied to an
epoch with another `-MT` target: the receiver's fresh `-E -MMD -MP` must
produce the current-target depfile, which is independently checked and
materialized. A trusted current-main proof executor must verify Make skipped
the physical `-c`, rehash the final object and current depfile, and observe the
executed image. Candidate-built `zcc` has no authority to make that decision.
Any missing pin, changed checker body, unknown source search, malformed target,
or failed fresh check keeps the path cold or blocked under the later policy.
The pinned first-source revision can be reused unchanged; a body edit requires
an administrator to qualify and install a new source image/pin before an
eligible record can exist. The current worker does not durably sign compiler
FAIL observations. A nonzero checker/compiler under otherwise eligible inputs
must block the request and cannot be silently treated as a cold miss.

The local probe exercises formatting only. Its synthetic root vector yields
`closure_sha3=65c84f25e8eb3be2c3b41a6f6946fbf05a1c5302d395819d05faf39f9adf9018`.
It launches zero compilers and avoids zero proof launches. It is not an
eligibility or performance witness.
