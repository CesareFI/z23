<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Non-mainnet legacy import boundary (2026-09-28)

## Observation

On an x86_64 AMD Ryzen 7 PRO 8840U laptop with GCC 16.1.1, source commit
`544ca32ae34e72e1d1b26a6c4d464556edc39c60` built the C23 public node.
At 2026-09-28T11:26:13Z, a fresh scratch datadir was started with `-regtest`,
non-default ports, and `-connect=127.0.0.1:44999`. The default `HOME` contained
a separate zclassicd datadir. Boot printed `LevelDB→SQLite UTXO migration from
$HOME/.zclassic/chainstate` and began copying that chainstate into the scratch
datadir. The scratch datadir reached 398 MiB before the test process was
stopped. This was a default cross-network source read that an isolated regtest
startup should not perform. The test did not edit the zclassicd datadir.

The same binary started a second fresh scratch regtest datadir with an isolated
`HOME`, `-nolegacyimport`, `-no-tor`, and the dead loopback peer. It answered
`getblockcount` with `0` and completed an RPC-requested graceful shutdown.
That control establishes the explicit refusal path; it does not establish the
new default.

## Change and acceptance

Argument finalization sets `no_legacy_auto_import` for regtest and testnet.
The existing boot gates then refuse the automatic mainnet header, block-file,
UTXO, and snapshot source paths. Mainnet retains its existing default. The
documented scratch-start command also names the legacy and Tor refusals.

`make -j12 t-fast ONLY=test_app_context` passed one registered group with zero
failures or skips. The group checks mainnet, regtest, and testnet parser
results.

At 2026-09-28T12:01:13Z, the candidate development binary (SHA-256
`81cdfd04df6c51a64dd5bed63d836140bf48f675fef5566b8e6fac3167a5de06`,
embedded source prefix `d09ada45a1e7`) started a fresh regtest datadir while
`HOME` still pointed at the host with zclassicd chainstate. It used `-no-tor`
and a dead loopback peer, but deliberately omitted `-nolegacyimport`. RPC
`getblockcount` returned `0`, RPC `stop` completed a graceful shutdown, the
boot log contained no legacy migration or block-link message, and the scratch
datadir measured 2.6 MiB. This verifies the default guard in a real boot for
the candidate development profile.

At 2026-09-28T12:24:15Z, the public C23 binary built from signed commit
`27629fedd5e344d8ccbe1329df0a3848aa9af0b3` (SHA-256
`d124cad7938b3ae4d45b96fb4ae2a7157017f4f923ce3347e8ab9256856c8bf6`)
started a new scratch regtest datadir with the normal `HOME`, `-no-tor`, and a
dead loopback peer, again without `-nolegacyimport`. `make -j12 z23` passed
the public C23 build gate. RPC `getblockcount` returned `0`; RPC `stop` led to
exit status 0 and `Shutdown complete.` The scratch datadir measured 1.8 MiB.
The log contained no UTXO migration source, chainstate copy, or block-file
link. The block-index loader printed its generic `Loading block index from
LevelDB...` label while initializing one genesis entry, and the legacy mirror
explicitly reported that it was skipped on regtest. Neither line reports an
import from the host's zclassicd datadir. The complete lint umbrella remains
separate from this boot acceptance.

## Exact-source receiver checks

Signed candidate `f6896f968243349a68f01406b8e4dbe07ce2e480` was exported
as a 13,788-byte Git bundle. The laptop and both receivers measured SHA-256
`4636f0c58c1cbbde23388ececdd44ebc58bd4b5856946aaee481e9bda518e646`.
Each receiver verified the bundle against commits already in its object store
and checked out the candidate in an isolated worktree. The x86_64 Linux
receiver, using GCC 14.2.0, passed the registered `test_app_context` group
with zero failures or skips. The arm64 macOS receiver, using Apple Clang
17.0.0, passed the same group and `make -j8 z23`; the public Mach-O binary had
SHA-256 `39773c15dfcaa3d02a72a7921fd2b629a05ae079de3417075b55fce389894d3a`.
The Mac worktree used the receiver's already checked-out pinned Tor commit
after a network submodule clone made no progress. Its first public build
refused a transient live-Make start-token lookup; the retry acquired the epoch
and passed. The receiver checks establish exact-source portability of the
argument guard, not a second live-boot observation.

The laptop also rebuilt the merged public candidate at that commit with GCC
16.1.1. `make -j8 z23` passed its C23 gate; the resulting ELF binary had
SHA-256 `86910811c6e63d888acbcc37fd3f2c807e922c285b2f267913983b2ad64d7d43`.
The merged `test_app_context` group passed with zero failures or skips.

## Final merged public boot

After integrating upstream through `e55ca12385d26e6f3ed5b1d47c47537e47a11b21`,
the safety branch at signed commit
`db04e4a8ec1755c1cb287d7fc444e24f7cb64019` passed
`make -j8 t-fast ONLY=test_app_context` and `make -j8 z23`. The public ELF
binary had SHA-256
`63db7fd9b580d6f8d76363e652d64572ac5ccc93c10e6b3c802d3f16b55d70e5`.
It started a new scratch regtest datadir with normal `HOME`, `-no-tor`, and a
dead loopback peer, without `-nolegacyimport`. RPC `getblockcount` returned
`0`, RPC `stop` completed with node exit status 0, and the log ended in
`Shutdown complete.` The scratch datadir measured 1.8 MiB. No legacy UTXO
migration, chainstate copy, or block-file link was logged. This is exact-binary
startup evidence for the merged candidate; it does not claim chain sync.
