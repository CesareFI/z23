<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Laptop three-host route qualification

At 2026-09-28T23:01:35-04:00 (2026-09-29T03:01:35+00:00), laptop A was
Linux 6.12.94-1-MANJARO x86_64 with GCC 16.1.1. The inspected checkout was
`cc1a92b843454757814e08ab497151959827259e`; its root `AGENTS.md` was
Git blob `fba498d083f0e5e47cd5a9d5ece98d04ad42a7ac`, 18,709 bytes,
SHA-256 `354158d24dbe6c942bab3a819a049ff26016e4551c043aac53e725b5f4abac24`.
B and C were two consenting development hosts, named here only by role.
No host name, private endpoint, key fingerprint, or credential is recorded
here.

| Route | Bounded authenticated command | Exact object result |
|---|---|---|
| A ↔ B | Both directions passed | A's Git-tracked object returned byte-identically; B's public-key object returned byte-identically |
| A ↔ C | Both directions passed | A's Git-tracked object returned byte-identically; C's public-key object returned byte-identically |
| B ↔ C | Direct commands passed in both directions with pinned host keys | B sent A's 18,709-byte object directly to C; after A's temporary source and listener were removed and B's scratch copy deleted, B fetched the object directly from C at SHA-256 `354158d24dbe6c942bab3a819a049ff26016e4551c043aac53e725b5f4abac24` |

The laptop initially had no SSH listener. A temporary user-owned OpenSSH
server bound only to loopback, accepted the two peers' existing public
Ed25519 keys, and forced a four-command allowlist for a 16 KiB scratch
object. Each peer reached it through a loopback-only reverse SSH forward.
The ephemeral server host key was carried to each peer over its already
authenticated link and matched byte-for-byte before use. The peer-to-peer
direct commands pinned each target's public host key obtained over the
authenticated laptop link. Private keys stayed on their original machines.
The server, forwards, temporary credentials, and scratch workspaces were
removed; both peers then reported zero reverse listeners.

At 2026-09-28T23:17:09-04:00 (2026-09-29T03:17:09+00:00), A archived the
exact `origin/main` source tree at
`9aebce61cfb70062ec2a9c3a21d7e9d48229bb43` into a 43 MiB gzip file.
Its SHA-256 was
`2e6ac05b2a3494826a837d4b58d1e50e3305c4029113e1a28a20999086cf7c6a`.
A sent the archive over authenticated SSH to B; B sent those bytes directly
to C using its own key and A-pinned C host key. Independent SSH sessions
from A to B and C reported the same SHA-256. Both receivers passed
`gzip -t` and found `AGENTS.md` and the package-admission command source in
the archive listing. At 2026-09-28T23:20:16-04:00
(2026-09-29T03:20:16+00:00), each receiver extracted the verified archive
into a new isolated source directory containing 9,469 regular files. No
received source was built or executed. Both peers reported GCC 14.2.0 on
x86_64 Linux. C lacked `rg`, so its listing check used POSIX `grep`.

At 2026-09-28T23:22:41-04:00 (2026-09-29T03:22:41+00:00), A packed the
exact main commit object and every object reachable from its tree into a
36 MiB Git pack, SHA-256
`19ebd8e0f1fb01ff467cee0c241d48ee1b0489b08cfbbc0b5f0229348397fb24`.
It followed the same authenticated A → B → C direct route. Each receiver
indexed the verified pack into a new isolated Git repository, marked that
exact commit as the shallow boundary, checked out its tree with Git
plumbing, and passed `git fsck --connectivity-only --no-reflogs`. Both
repositories reported clean `main` at
`9aebce61cfb70062ec2a9c3a21d7e9d48229bb43`. Neither receiver used a
central Git server to obtain this source identity. The shallow boundary
does not carry earlier history.

Peer aliases did not resolve through DNS on the other peer or on this laptop
at the time of the test. Existing authenticated SSH master sockets exposed
the numeric peer routes used for the direct B ↔ C qualification. That route
discovery is still an operational seam: this experiment does not establish a
durable native endpoint-discovery mechanism.

At that checkpoint, this was machine-access and exact-byte transport evidence.
Laptop A had no live local Z23 node. The route alone did not prove a C23
candidate lifecycle, fetched carrier verification and admission, public
serveability, independent build, remote observation, or a product-level
producer-disappearance journey. A's SSH control connections remained
available for orchestration while its temporary source and listener were
absent.

At 2026-09-28T23:36:36-04:00 (2026-09-29T03:36:36+00:00), B and C had each
built `z23` from their own exact `9aebce61c` checkout. Both builds passed
the `c23-node` gate with `zcc cc` selecting GCC 14.2.0. Their node binary
SHA-256 values were respectively
`6a32190da156f21aa9390dfdaebf2f1ba687541373a010002d2acb2492e36f50`
and
`55f13c0f52c15dfd07e236b6497b2d1c1e5d3e5057e901287fb4caf589941e28`.
The different binary hashes are recorded, not treated as a reproduction
match. Each binary's live `discover schema zcode.package.admit` reported
`transport_root,datadir` as its allowed input keys. Both nodes were joined
in new isolated regtest datadirs through `zcode.node.join`; that command
wrote `packagehost=1` and `buildworker=1` and reported GCC 14.2.0. A later
check found that B and C each reported
one ordinary P2P peer in their own live RPC `getpeerinfo` result over the
direct B ↔ C route. The default DHT identity was unavailable, as expected
for the no-identity swarm path. No package transfer or independent package
build had yet been observed at this point.

Integrating current `origin/main` at `a00a1f1a71c5eaa804f559ae74e65635accd100d`
preserved the upstream split make reader and the branch's platform file-shape
checks in the facts consumer. The generated capability inventory was rebuilt
from that integrated tree. `make check-capability-inventory-generated` passed
with 1,513 capabilities and 1,177 registered roots resolved; `git diff
--cached --check` passed after removing one upstream blank line at EOF.

## Physical package journey

By 2026-09-29T00:09:59-04:00 (2026-09-29T04:09:59+00:00), all three isolated
Linux nodes had built the exact `9aebce61c` source and exposed the carrier
admission command. A used GCC 16.1.1; its `z23` SHA-256 was
`cb541a6d5f1649266a753a275ea40be5766cfbe9ab3626bf83b02b5108bf6068`.
The different node binary hashes across machines are build-environment facts,
not a package-reproduction claim.

A materialized the reviewed `zprng` C23 package from historical commit
`8fb7b18b15a13e43dcc70c86e7b79f8c850ad897` in an isolated source
directory, generated a local 0600 offline author key, prepared and signed its
release, and committed the exact package root
`91e9406a1016bcc224bb5e229377b1841e21c8ede1e0a00bc0d45d1989c41563`
and signed carrier root
`0c726f010c50b2e6e0b1843a6ffa6ef48612005b7092f732136dbe028ddbb86d`.
The package root matched the historical source fixture. The private author
key stayed in A's isolated scratch directory.

With A and B connected as ordinary regtest peers, B's live store reported
both roots tracked and complete: the package had 7/7 chunks and 10,654/10,654
bytes; the carrier had 10/10 chunks and 11,442/11,442 bytes. A then stopped
gracefully. B reported zero peers while its package remained complete. Before
carrier admission, B's `zcode package add plan` refused the package because no
release named that root. B stopped its node, admitted the exact signed carrier
locally, and reported 7 source chunks reused from 7 fetched CAS objects.

B restarted as a package host. C's clean download records pulled both roots
from B over their direct ordinary P2P connection while A's process remained
stopped. C's live store reported both roots complete, with one peer on each
side of the B ↔ C link. After C stopped, its pre-admission add plan refused
the missing release in the same way. C's explicit admission verified the
carrier and reused all 7 fetched source chunks. Fetching and serving had not
installed or run source on either receiver.

B and C independently ran `zcode package add plan` and `commit` through their
own confined package verifiers. Both activated the exact package root and
produced `libzprng.a` with SHA-256
`34c86b6a89cf8d76d84012c88091317ecc98f5f37ac2f1c55cb9ffb8d32e7fe3`.
Their local build-report file hashes differed, so this equality claim is for
the archive only. A's GCC 16.1.1 build produced archive SHA-256
`0385378c1770bc30ca635f1ec943f17c4ae722b8c39c06b963a28cdddeebd167`;
it is not byte-identical to the GCC 14.2.0 archives. `zcode package checkout`
reconstructed the app source from each node's own store. Each node compiled
the demo against its own installed archive with C23, `-Wall -Wextra -Werror
-pedantic`, ran seed 42 for five values, and produced output SHA-256
`d45346ccf60d63ccd68b801c19ce1860cb6fa17c44e90e605c0463b15ced31fb`.

A's first explicit add commit refused because `make z23` had not built the
separate package verifier. `make zclassic23-package-verify` supplied that
helper; retrying the same plan passed. This was a visible fail-closed operator
dependency, not an implicit execution path. All three scratch nodes were
stopped after the observation. No canonical datadir, wallet funds, on-chain
transaction, or production node was used. The isolated datadirs and checked
out application sources remain available for inspection.
