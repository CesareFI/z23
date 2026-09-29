# Commons journey across three physical hosts (2026-09-29)

`make commons-multihost-acceptance` drives the twelve-step Commons journey
across three consenting hosts. Each host runs its own isolated regtest
datadir, and every node reaches the others over its own P2P port. No tunnel
carries node traffic.

## Topology

| Role | Host | Node | Duty |
|------|------|------|------|
| A | driver host | requester and publisher | asks, writes the missing behavior, accepts, publishes; killed in step 12 |
| B | second host | build worker | proves remotely, reproduces the source, signs receipts, exports the object carrier |
| C | third host | latecomer | joins after A exits and learns only from B |

The driver copies one set of binaries to B and C and checks each copy by
sha3 before any node boots. The production nodes on B and C were not
touched: their service main PIDs were recorded before the runs and were the
same afterwards.

## Source

- Base: `6636fe270f`
- Run 3 (PASS): `a7fd5574f5`
- Runs 5 and 6: `9d1a5cd587`. The `z23` sha256 was
  `103aeef7027d09b0a391cbe77d60d668c44f59241bafd8ebe309f594c368183d`, and it
  matched on all three hosts.

## Runs

| Run | Source | Result | Stopped at |
|-----|--------|--------|------------|
| 1 | base | FAIL | step 12: carrier rebuild judged by the `cc --version` banner |
| 2 | + carrier fix | FAIL | step 8: the remote proof result was lost when the session dropped |
| 3 | + session fix | **PASS 12/12**, 479 s | none |
| 4 | same | FAIL | bring-up: node A anchor funding reported `Insufficient funds` |
| 5 | + wallet fix | FAIL | overlay: node B stuck in `finding_peers` (two-node dial deadlock) |
| 6 | same | FAIL | overlay: same as run 5 |

### Run 3 (PASS) step results

Every step verdict was PASS, and the verdict line reported
`"steps_proven":12,"steps_total":12,"complete":true`. The whole run took
479 s. Durations reported inside the run:

| Step | Result |
|------|--------|
| 1 guide | PASS |
| 2 commons package present | PASS |
| 3 B fetches peer-to-peer, bytes stay inert | PASS (15481 bytes from A, no registry) |
| 4 work start, reuse searched first | PASS |
| 5 use: explicit local admission | PASS |
| 6 work run: only the missing behavior | PASS (built and tested on A 67 s after the ask) |
| 7 work show | PASS |
| 8 work accept: exact bytes travel | PASS (B re-derived the source root in 7 s and signed it) |
| 9 use on the second node | PASS (ask to running program: 91 s) |
| 10 object-set carrier | PASS (2 objects, B to C, as one package) |
| 11 change existing software | PASS (143 deg/s before, 239 deg/s after, as its own root) |
| 12 publisher gone | PASS |

- **Publisher kill:** node A was killed. Node C held none of the survival
  objects and had executed no work before A exited.
- **C from B:** node C fetched the accepted application from B alone (22074
  bytes) and reproduced and ran it. It measured the same 239 deg/s that B
  measured. The carrier moved 2 objects from B to C.
- **Receipts:** 2 signed source-reproduction receipts were consumed. The
  tamper refusals fired by name: `LANE_NOT_ACCEPTED`, `TARGET_UNRESOLVED`,
  `CONFIRMATION_IDENTITY_STALE` and `SOURCE_PACKAGE_CHECKOUT_REFUSED`.
- **Central services contacted:** 0.
- **Toolchain:** hosts B and C have different toolchain capsule roots, so the
  verdict reports `carrier_toolchain=different_capsule`. It also reports
  `cross_capsule_objects_reused=0` and `zero_compiler_rebuild=false`: C
  rebuilt under its own capsule and reused none of B's objects.

### Runs 5 and 6: wall times on `9d1a5cd587`

| Phase | Run 6 wall time |
|-------|-----------------|
| bring-up (three datadirs, binaries copied and verified) | 19 s |
| mining, custody restart, backup | 48 s |
| identity anchors and delegations | 8 s |
| steps 1-2 and staging | 2 s |
| overlay: authenticated session A <-> B | 27 s |
| overlay: wait for B's build worker to admit work | 91 s, then refused |

## Faults found and fixed

1. **The carrier verdict compared compiler banners.**
   - **Cause:** Hosts with the same `cc --version` but a different
     binutils patch have different toolchain capsule roots. The capsule root
     is bound into the fastobj cache key and the build receipt.
   - **Fix:** The driver now reads `zcode work toolchain` on B and C. With
     the same capsule, it still demands full reuse, zero misses and an
     identical receipt. With a different capsule, it demands zero reuse, a
     full rebuild and a different receipt.
   - **Regression test:** `tools/dev/commons-journey-ordering-selftest.sh`.
2. **A remote proof result was stranded when its session dropped.**
   - **Cause:** Work-session ids are connection-scoped and never return.
     A worker that finished while its session churned queued the RESULT to a
     dead session, and the requester kept waiting until its deadline.
   - **Fix:** A lost session now makes the requester re-dispatch the same
     binding to the worker's new session, keeping the original deadline, and
     supersedes the lost track.
   - **Regression test:** `zcode_dev: a dropped session moves the exact
     binding to the worker's new session` in `test_zcode_dev_objects`.
3. **The boot wallet catch-up read block bodies from the wrong directory.**
   - **Cause:** On regtest and testnet, block writers store bodies under
     `<datadir>/<network>/blocks`, but the boot catch-up rescan read
     `<datadir>/blocks`. After an unclean stop the rescan failed every body
     and still moved the wallet to the tip. Each coinbase then kept the depth
     it had at the last flush, so the wallet refused to spend coins its own
     vault reported as mature.
   - **Fix:** The catch-up now reads the network-specific directory.
   - **Regression test:** scenario E in `test_wallet_rescan_coverage`.

## Open blocker: two nodes on two hosts cannot both reach the tip

Runs 5 and 6 stopped at the same point. B was stuck in `finding_peers` with
one inbound and zero outbound peers, so its build worker declined every
action (`sync_not_at_tip`). Three policies combine:

- Same-IP inbound eviction when an outbound connection to that IP exists.
  This is in the sealed core and exempts only loopback.
- Header sync begins only from outbound peers, as an anti-eclipse rule.
- The periodic transition to the tip is not reachable from `finding_peers`.

Two nodes on two routable hosts that are configured to dial each other end
up with one TCP connection, and the side left inbound-only cannot sync. Run 3
passed because the dial race went the other way. Getting the full journey to
pass reliably needs an owner decision. Either the sealed-core eviction must
change (for example, exempting configured peers), or the anti-eclipse sync
gate must admit an authenticated, configured inbound peer. Neither change was
made here.

Separately, in run 4 node A's own shutdown hit `database is locked` on
node.db while the wallet flush was retrying. The harness's TERM grace then
expired, which caused the unclean stop. Fault 3 makes the node recover from
that stop. The lock contention itself remains open.
