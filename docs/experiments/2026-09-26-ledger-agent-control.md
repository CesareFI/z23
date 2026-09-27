<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Offline Ledger Blue review contract for Z23 agents

Recorded: 2026-09-26T22:38:20-04:00 / 2026-09-27T02:38:20+00:00.

## Objective

Give an automated Z23 caller a repeatable offline check of the exact C23
Review app controller, with a machine-readable distinction between simulated
behavior and a physical Blue response.

## Result

`zcl-tx-review --json --simulate-app --branch-id 0x76b809bb` completed on
the 245-byte published ZIP-243 transparent fixture. Its JSON reported
`app_simulated:true`, `blue_parsed:false`, `simulated_zip243_matched:true`,
`blue_zip243_matched:false`, and `signing_ready:false`. The simulator sent
the identity, begin, chunk, digest, and summary commands; it compared the
summary and digest with the host and traversed every public output page.
The mode has no USB path. The CLI rejects `--simulate-app` combined with
`--blue` before attempting a device open.

A C23 subprocess test generated the binary fixture and checked success,
the mutually exclusive options, and an unreadable-file error. JSON failures
returned stable `error` codes and nonzero exit status. Clang 22.1.6 Debug
with AddressSanitizer and UndefinedBehaviorSanitizer passed 14/14 local
CTest cases. GCC 16.1.1 Release passed 14/14. The repository's cyclomatic
complexity gate passed at cap 15.

## Limits

This tests the C23 controller and screen text. It does not run BOLOS, USB
interrupts, BAGL rendering, touch, EXIT, key derivation, or signing. The
physical Review 0.4.2 candidate remains uninstalled and unpinned.
