<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Sky Combat — the game in this repo

`apps/skycombat/` ships an aerial combat game in a cyberpunk city with
one local player, four AI aircraft and a single view following the local
aircraft. Guns, powerups and aircraft AI are wired into that executable.
It is the owner's own game (Apache-2.0, imported from
`full-node-firewall-fly-over`) rewritten here as C23 that compiles under the
repo's own-code flags. It is an application, not part of the node: it links
nothing from `core/` or `engine/`, and the node binary links nothing from it.

## Build and run

```
make game          # links build/bin/z23-skycombat  (needs X11 + GL headers)
make game-check    # compiles the game and raylib to objects only, no window
./build/bin/z23-skycombat
```

Both are **opt-in**: the default build, `build-only`, the push proof and every
lint gate ignore them, so a headless or header-less host is never blocked by
the game. `make game` probes for the platform headers first and, if they are
missing, prints one typed line — `game_platform_headers_missing: <what>` — and
exits 2 instead of a wall of compiler errors. `make game-check` needs no
window and is what a headless gate box proves. These game-only goals build no
node vendor archive or embedded Tor archive; mixed game and node goals retain
the node's vendor and Tor prerequisites.

The playable simulation and its mixed update/draw entrypoint compile with
`-ffp-contract=off`. Their sources also set `STDC FP_CONTRACT OFF` on Clang;
GCC enforces the build flag. Simulation raymath helpers are local to those
objects, so rendering's external helpers cannot change their rounding.
Dedicated view sources, cosmetic world generation and raylib keep their
existing floating-point policy. This removes contraction as a replay
difference; native math-library results can still differ between platforms.

## Controls

Gamepad (ASTRO C40 / PlayStation layout; the mapping is locked in
`include/sky_combat/models/input_model.h`): left stick flies, inverted; right
stick aims the camera; **R2** fires guns; **L2** fires missiles; **L1/R1**
barrel-roll; **gas** boosts; **brake** throttles up. Keyboard, used when no
pad is connected: **WASD** or the **arrow keys** fly, **space** boosts, **left
ctrl** fires guns, **left shift** throttles up. The playable binary acts
on fly, boost, throttle and guns; missiles and barrel rolls are in the input
model but not yet bound in it.

## Playable scope and incomplete integration

The selected entrypoint is `apps/skycombat/src/sky_combat_multiplayer_ultimate.c`,
chosen by `SKYCOMBAT_GAME_MAIN` and the `SKYCOMBAT_GAME_SRCS` link closure in
the Makefile. Imported demo mains and model APIs are available source, not
additional features of `build/bin/z23-skycombat`. This is one local player
with four AI aircraft in a single view; it does not provide local split-screen.

The match starts once after rule configuration. Each live tick snapshots team
kill totals and advances the timer. On reaching the score or time limit, scores freeze
and the winner remains fixed. Normal shutdown finishes an unfinished match.
An ended-match screen, a new-round flow and match save/reopen are not wired.
These ledger guarantees do not imply that aircraft simulation stops at match end.

Building entry is not yet wired: the playable link closure excludes the
building system. The main draws a cyberpunk city but does not update ground
AI. Neither imported building APIs nor other demos establish those features
in this executable.

Sky Combat is single-machine today. The playable link closure has no network
transport. This document promises no fleet service integration or delivery lane.

## What is proved, and what is not

`make t-fast ONLY=skycombat_models` runs the headless model tests
(`tests/harness/src/test_skycombat_models.c`): the aircraft physics step,
weapon cooldown and wing selection, pickup reservations, match lifecycle and
world bounds, with fixed inputs and private shims for seven raylib drawing,
random and time calls. An additive check reads this public launch document
and refuses obsolete scope claims. Rendering, physical input devices and the
window itself have no automatic test in this group.

`make t-fast-exact ONLY=skycombat_fp_contract` builds and tests the actual
game aircraft object without window-system headers. Its known-answer yaw
step rounds to zero with separate binary32 operations; contraction preserves
a nonzero residual. The test also checks the explicit fused reference, so
the input's ability to distinguish those policies is asserted.

## Licences

The game is Apache-2.0 (`LICENSE` at the repo root) and every imported file
carries the repo header. raylib is vendored under `vendor/raylib/` under its
own zlib licence, origin commit and digests pinned — see the dependency
section of [BUILD.md](BUILD.md). No submodule, no package manager, no system
raylib. Source files that could never compile upstream (headers that exist
nowhere, SDL2/GLUT/GLEW demos, the GDB-proof pipeline) were not imported; the
import commit for `apps/skycombat` names each one.
