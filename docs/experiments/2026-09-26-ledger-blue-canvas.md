<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Reusable C23 Ledger Blue screen canvas

Local time: 2026-09-26T23:25:14-04:00

UTC: 2026-09-27T03:25:14Z

## Question

Can Review's BAGL font drawing and PNG output become a reusable Blue app
preview library without changing any Review screen pixel?

## Method

The prior `a1b064e50` source was built in a separate detached worktree.
The candidate and prior host simulators rendered the same ZIP-243 transparent
vector 3 fixture in regular, large-text, dark, and large-text dark modes.
Each corresponding PNG was compared byte for byte. The new canvas contract
was tested for image dimensions, color values, clipping, font metrics,
unsupported text, and bounds rejection.

Both complete local app suites ran with Clang 22.1.6 in Debug with AddressSanitizer
and UndefinedBehaviorSanitizer, and GCC 16.1.1 in Release. CPU: AMD Ryzen 7
PRO 8840U with Radeon 780M Graphics. The cyclomatic complexity ratchet has
cap 15.

## Result

| Preview mode | Identical PNG files |
| --- | ---: |
| Regular | 5/5 |
| Large text | 29/29 |
| Dark | 5/5 |
| Large text and dark | 29/29 |

Clang Debug: 15/15 tests passed. GCC Release: 15/15 tests passed.
Cyclomatic complexity check passed. The canvas adds no ARM app code, so the
0.4.3 device image has not changed or been installed in this experiment.

## Limit

The canvas renders reviewed SDK font tables on the host. It does not execute
BOLOS, emulate touch or USB, or establish physical pixel equality on the
Blue. The Review app remains unable to sign transactions. Actual Sapling
memo display and signing require separate consensus and device tests.
