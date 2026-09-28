<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Replay canary start-stamp synchronization

Recorded: 2026-09-28T04:46:01Z (2026-09-28T00:46:01-04:00).
Host: x86-64 AMD Ryzen 7 PRO 8840U, Manjaro Linux, GCC 16.1.1.

The cold exact proof of commit `e5343184426d14fba3c82da8d812e9853d079d65`
against base `b737a2703ec0f59d5835856d437c49db4a0dea64` ran 1,211
groups with zero skips. Its replay-canary verdict group failed because the
parent inspected a seeded stale PASS 600 ms after spawning a child, before the
child reached `reset_verdict`. The isolated group passed in 2.1 seconds with
no competing proof load. The original failure demonstrates that elapsed time
alone did not establish the child had started the canary run.

The focused reproduction delays child startup by 750 ms, leaving the original
600 ms observation in place. `make -j4 t-fast ONLY=test_replay_canary_verdict`
then failed 1/1 groups cold, with zero skips and a 2.9 second test body. The
RED log SHA-256 is
`22dbd1aba51003086b74a97dba04dc85fc62255d7715cd325687971b7af5545c`.

The test now waits at most 10 seconds for the canary's existing run-start
stamp. An absent stamp fails the test. Once the stamp is present, the test
still requires the stale PASS to be absent, kills the child, and requires no
fresh PASS to exist afterward. The 750 ms delay remains as a regression
fixture. The test also clears any prior stamp at the fixture path before
launching the child. The final focused command passed 1/1 groups cold with
zero skips and a 2.6 second test body. The GREEN log SHA-256 is
`50d70f72781b7c07b883aa8911891d79e7fffff15a6231e51b157fb936889576`.

The change is confined to the test. The production canary's reset, stamp,
sentinel, and refusal behavior are unchanged.
