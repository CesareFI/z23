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
- Runs 8 and 9: `ce0a212fa3` (configured-inbound sync). The `z23` sha256 was
  `900d69a14200ed25041573bd8af1c24ee81a5882e8a24d2164690b1ed738de87` on all
  three hosts. Toolchain capsule roots:
  - A and B: `5c82d3bc9d023caaf15a93b0db74f97aa0430f0c6458ae87b5d20aea6b3fcc8a`
  - C: `b0a1234afb88c1cbfb507b3d41c0f4b4422a428de33637b272818d33ce67d732`

## Runs

| Run | Source | Result | Stopped at |
|-----|--------|--------|------------|
| 1 | base | FAIL | step 12: carrier rebuild judged by the `cc --version` banner |
| 2 | + carrier fix | FAIL | step 8: the remote proof result was lost when the session dropped |
| 3 | + session fix | **PASS 12/12**, 479 s | none |
| 4 | same | FAIL | bring-up: node A anchor funding reported `Insufficient funds` |
| 5 | + wallet fix | FAIL | overlay: node B stuck in `finding_peers` (two-node dial deadlock) |
| 6 | same | FAIL | overlay: same as run 5 |
| 7 | + configured-inbound sync, probe socket non-blocking | FAIL | overlay: B's identity probe returned at once, so B stayed in `finding_peers` |
| 8 | `ce0a212fa3` | **PASS 12/12**, 617 s | none |
| 9 | same | FAIL | step 11: the remote proof session was lost to connection churn |

Wall times are the driver's own run time; queue time for the shared build
slot is excluded.

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

4. **The node left holding only the peer's inbound session never synced.**
   - **Cause:** The sealed core evicts a same-IP inbound once its own
     outbound to that IP completes, and header sync began only from outbound
     peers. Two nodes told to dial each other could end with one connection,
     and the inbound-only side stayed in `finding_peers`.
   - **Fix:** An inbound session may serve headers when its own Noise
     session authenticated the static key that this node learned by
     completing Noise XX as initiator to an operator-named `-addnode`,
     `-connect`, `-addnode-file` or `addnode` RPC target at the same IP.
     The key comes from an outbound session to the exact target, or from a
     bounded identity probe that completes the handshake and closes before
     `VERSION`. The source IP, `addr_from` and gossip grant nothing, and
     plaintext, loopback and onion sources never qualify. Every session is
     checked against its own handshake. The rule and its argument are in
     `engine/services/include/services/configured_sync_peers.h`.
   - **Same limits as an outbound peer:** the peer goes through the same
     header and block validation, download windows and misbehaviour scoring.
     The core applies body-stall rules C and D only to outbound peers, so
     the stale-header predicate applies them to a configured inbound sync
     peer. The one difference left is rule B, the core's outbound-slot
     rotation, which has no inbound equivalent.
   - **Regression test:** `test_sync_service_configured_inbound.c` in
     `sync_service`. It drives real connection managers, real Noise XX
     pairs and the core eviction through all 24 A/B handshake interleavings,
     a reconnect, an impostor key at the configured IP, a plaintext session
     and publisher loss, with an injected prober and clock. On the old gate,
     16 of the 24 interleavings leave a node without a header source. One
     further case runs the real prober against a loopback listener; run 7's
     non-blocking socket fails it.
   - **Observed in run 8, node B:**
     `configured sync peer identity probe completed Noise XX with target=<A>:20028`,
     then
     `configured-inbound header sync: peer=<A>:49584 id=2 proved over its own Noise session the identity this node authenticated at an operator-named -addnode/-connect target; ...`.

## Open blocker: reconnects between two configured peers destroy both sessions

Run 9 reached step 11 and then lost its remote proof. B's log shows the
cycle every 30 s:

1. A dials B. B holds only that inbound session, which is bound and serves
   headers.
2. B's `-connect` loop dials A again. It waits only while its outbound
   count covers every connect target
   (`core/modules/net/src/connman_dialer.c:740`), and an inbound session
   never satisfies a target (`connman_node_conflicts_with_target`,
   `core/modules/net/src/connman.c:692`).
3. A processes B's `VERSION`, answers `VERACK`, and evicts B's inbound,
   because A holds an outbound to B's IP.
4. B receives that `VERACK`, completes its outbound, and evicts A's inbound
   (`core/modules/net/src/connman_zcl23_dial.c:154`).
5. Both sessions are gone. A redials within a second, and the cycle repeats.

Whether step 4 happens depends on whether B processes the `VERACK` before
the close. Run 8 passed on that timing, and run 9 did not. The regression
test shows the same thing without timing: 8 of the 24 handshake
interleavings leave no surviving session. The configured-inbound rule cannot
help when no session survives.

The fix belongs in the sealed core. Either rule would do:

- A connect or addnode target counts as connected while an inbound session
  from its IP carries the target's authenticated Noise identity.
- Both nodes apply one deterministic tie-break when both directions exist,
  such as keeping the connection dialed by the lower static key. Each side
  knows both keys after Noise, so both evict the same connection.

The engine cannot suppress a core dial or eviction, so this change needs an
owner decision.

Separately, in run 4 node A's own shutdown hit `database is locked` on
node.db while the wallet flush was retrying. The harness's TERM grace then
expired, which caused the unclean stop. Fault 3 makes the node recover from
that stop. The driver now gives each stop the node's own shutdown grace
before escalating to KILL. The lock contention itself remains open.
