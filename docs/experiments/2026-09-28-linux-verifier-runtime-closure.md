<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Linux verifier runtime closure on the laptop

At 2026-09-28T14:58:10Z, source commit
`b580659d957d1f9022add68c9b954e3224d6b6c4` was tested on an x86_64
AMD Ryzen 7 PRO 8840U laptop with GCC 16.1.1. The checkout had the upstream
proof CAS and verifier-store changes through `8ef06fa6b`.

`make -j8 t-fast ONLY=test_build_fabric` passed the `test_build_fabric`
group, including incomplete proof transfer and quota preservation. The
separate `test_build_fabric_attach` group failed four cases with
`executor-runtime-closure-missing`. Its automatic retry also failed. A later
`make -j8 t-fast ONLY=test_build_fabric_attach` with no competing public link
failed the same four cases. Both runs had zero skipped groups; the second
run's receipt was 1/1 groups failed in 85.8 seconds. Both logs were kept on
the laptop and are not reproduced here.

The dev verifier at `build/bin/zclassic23-package-verify-dev` reports 144
lines and 9,659 bytes from `/usr/bin/ldd`, including the virtual DSO line.
`readelf -d` shows direct GTK 3 and WebKit2GTK dependencies. The dev verifier
links the full dev object graph and `GTK_LIBS`/`WEBKIT_LIBS` in `Makefile`.
`bfat_hash_loader_line` refuses after 64 resolved entries. The observed
closure therefore exceeds that bound even when the host is idle. This is a
diagnosis of the local refusal, not a successful attach or reusable proof.

At 2026-09-28T15:45:11Z, the dev verifier was rebuilt from commit
`7970320d81010efd10fb6c95aaa3373070a211b5` with a Makefile change that
links its existing no-GUI wallet fallback and omits GTK and WebKit. `ldd`
then reported six lines, and `readelf -d` reported no direct GTK or WebKit
dependency. `make -j8 t-fast ONLY=test_build_fabric_attach` passed 1/1 group
in 19.0 seconds, with zero failures and zero skipped groups. The group covers
sealed verifier A-B-A execution, poisoned donor refusal, corrupt or missing
CAS input refusal, duplicate attachment, and independent reproduction.

The 64-entry refusal, root-owned path checks, and byte hashing remain
unchanged. The successful attach is scoped to this Linux host and its exact
loader closure; other hosts must measure and pass their own closure.
