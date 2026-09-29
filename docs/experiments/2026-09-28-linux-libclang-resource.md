<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Linux libclang sensor qualification (2026-09-28)

## Host and baseline

- Host: x86_64, AMD Ryzen 7 PRO 8840U, 16 logical CPUs.
- Compiler: GCC 16.1.1 (`cc (GCC) 16.1.1 20260430`).
- Observation time: 2026-09-28T10:33:01Z (2026-09-28T06:33:01-04:00).
- Baseline source: `fdb339a50330b3ef376f210b0d4f652d06ac4921`.
- Installed C API: `/usr/include/clang-c/Index.h`,
  `/usr/lib/libclang.so.22.1`, `/usr/lib/libLLVM.so.22.1`, and
  `/usr/lib/clang/22/include/stddef.h`. The checked paths and files were
  root-owned and not group- or world-writable.

The baseline `make clang-manifest` failed with `libclang C API not found`.
With explicit `CLANG_MANIFEST_LLVM_DIR=/usr` and
`CLANG_MANIFEST_SYSTEM_LIB=/usr/lib/libclang.so.22.1`, the sensor built, but a
minimal C23 translation unit including `<stddef.h>` exited 3: libclang could
not find that header. Adding `-resource-dir=/usr/lib/clang/22` to that emit
made it pass. The baseline uncached suite, run with those explicit build
paths, ran 1,210 of 1,221 groups in 2,058 seconds: six failed, 19
self-skipped, and nine passed only on solo retry. `test_semantic_sensor` was
one of the six failures and reported the missing `stddef.h` in its fixture.
That suite is diagnostic evidence, not an acceptance pass.

## Candidate observation

The Linux x86_64 fallback now selects the installed `/usr` C API and pins
libclang 22.1, libLLVM 22.1, and Clang's resource directory. The build and
proof worker check the root-controlled paths. The proof helper digest includes
the selected libclang and libLLVM bytes. The sensor appends the resource path
only when the caller supplied none; the manifest records the resolved resource
directory and hashes each header it read.

`make clang-manifest` completed without environment overrides. `readelf -d`
reported `NEEDED libclang.so.22.1` and no RPATH or RUNPATH; `ldd` resolved
libclang and libLLVM under `/usr/lib`. The same minimal C23 emit passed without
a resource flag with root
`bb0530965d1c5811009990bac851f0b1514ee1dc95fccba905b4e95591b5444e`.
An explicit nonexistent resource directory still exited 3 with the missing
`stddef.h` error. The emitted manifest named
`@sys/usr/lib/clang/22/include` and recorded SHA3-256 file roots for the
resource headers it read.

Focused registered tests from this checkout:

| Command | Result |
|---|---|
| `make -j12 t-fast ONLY=test_semantic_sensor` | 1 group passed, 0 failed, 0 skipped; 89.1 s |
| `make -j12 t-fast ONLY=test_dev_proof_stress_env` | 1 group passed, 0 failed, 0 skipped |
| `make -j12 t-fast ONLY=test_semantic_facts_live` | 1 group passed, 0 failed, 0 skipped; 1.8 s |
| `make -j12 t-fast ONLY=test_semantic_consumer_live` | 1 group passed, 0 failed, 0 skipped; 18.1 s |

These observations establish local build and focused semantic behavior on
this host.

## Independent Linux receiver

At 2026-09-28T10:59:34Z, a separate x86_64 Ubuntu host running GCC 14.2.0
and Clang 20.1.8 imported the signed candidate commit
`feca7a171cd4a5e716c88be79829adf8126206d0` from a 7,752-byte Git
bundle. Sender and receiver both measured bundle SHA-256
`6b81eca232dd0455f2de620773d15450fd510bb62a9a7ce3e384b5eb818d3187`.
The receiver verified the bundle against locally present base commits and
checked out the exact commit in a clean, isolated worktree. Its existing dirty
checkout was not changed. The receiver's own LLVM 20 installation supplied the
sensor; `make -j12 clang-manifest` passed and produced a sensor binary with
SHA-256 `f9dafd4beaf8be918d5a810937d469df0e6b1870825e57cd0c84c4c2db349b11`.

The receiver ran the registered `test_semantic_sensor` and
`test_dev_proof_stress_env` groups from that worktree. Both passed with zero
failures or skips. This independently reproduces the focused semantic and
proof-runtime tests on another compiler and LLVM layout. It does not yet
constitute a signed Z23 proof receipt or reproduce a complete application
release.

## Independent macOS build

An arm64 macOS receiver fetched the two required base commits into Git's
object store, then imported the same SHA-256-verified bundle into an isolated
worktree. The receiver verified the signed commit using the sender's explicit
public allowed-signer policy without changing its normal checkout policy. At
2026-09-28T11:02:18Z, `make -j8 z23` passed with Apple Clang 17.0.0. Its
`c23-node` gate reported C23 sources, pinned static project dependencies, and
Apple system runtimes only. The resulting arm64 Mach-O binary had SHA-256
`dc4f666f0b1da80dc2e0b335f67e7ba3328cab8c7d9b7018f24a787f87acb977`.
The isolated source worktree remained clean. This is an exact-source native
build observation, not a chain-sync or application release acceptance.

## Final bundle and local gates

The later signed source commit `544ca32ae34e72e1d1b26a6c4d464556edc39c60`
was exported as an 11,351-byte Git bundle with SHA-256
`6b77081c09640635e3af7291ea381f4ccae475ba3734ecc21348a99174975561`.
New isolated worktrees on both receivers imported the exact bundle and verified
the commit signature. The Ubuntu receiver passed `clang-manifest`,
`test_semantic_sensor`, and `test_dev_proof_stress_env`; the arm64 macOS
receiver passed `make z23` and the native C23 build gate. The receivers did not
alter their ordinary checkouts. These are independent build observations for
the bundled source, not a signed Z23 proof receipt.

On the laptop, `make -j12 lint-fast` passed all 33 gates after the final
portable path changes. `make check-windows-cross-syntax` passed 2,376 MinGW
translation units with zero failures. The registered
`test_dev_proof_stress_env` group passed all three cases. The generated
capability-inventory self-test passed with 1,512 capabilities and 1,174
resolved registered test roots. `CC=cc make tor-full` rebuilt the local Tor
archives; `CC=cc make check-tor-provenance` then passed archive, commit, and
compiler identity checks. The first complete lint run failed six checks.
Five were corrected or passed on targeted rerun. The build-epoch self-test
had exceeded its 600-second budget under shared load; its solo rerun passed.
After the portable scan capability row and code-index gate scheduling were
corrected, a complete `make -j12 lint` run on signed commit
`74dea2bf4596dacf85ec69e7f9e0d3daddc98747` passed all 213 gates.
