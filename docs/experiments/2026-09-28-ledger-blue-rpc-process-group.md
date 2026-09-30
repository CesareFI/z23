<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Ledger Blue host RPC descendant cleanup

Date: 2026-09-28T18:40:19-04:00; UTC: 2026-09-28T22:40:19+00:00.
Host: AMD Ryzen 7 PRO 8840U with Radeon 780M Graphics. Compiler: Clang
22.1.6. The Ledger Blue device image is unchanged.

The host chain-tip and unspent-output checks execute a local RPC program.
The seven-second deadline added earlier reaped the direct child, but the
child could fork a descendant that inherited stdout. If that descendant
kept the pipe open, the host timed out and killed only the already-exited
direct child. The descendant could continue running after the failed check.

The regression child forked a descendant, sent a valid chain-tip reply,
and exited. The descendant held stdout, slept nine seconds, and wrote a
marker. The host rejected the check after its seven-second deadline, then
the test waited three seconds and inspected the marker. Before the fix, the
test failed after 24.12 seconds because the marker had been written. The
first two existing seven-second timeout cases account for most of that
duration.

The RPC child now creates a distinct POSIX process group before redirecting
stdout or executing the RPC program. On failure or timeout, the host sends
SIGKILL to that group, also targets the direct child as a fallback, and
reaps the direct child. The descendant regression then passed in 24.06
seconds. The existing direct-child timeout and ordinary successful reply
tests continued to pass. The helper's complexity cap remains 15 without a
baseline change.

The complete Release suite passed 59/59; the AddressSanitizer and
UndefinedBehaviorSanitizer Debug suite passed 59/59 with
`ASAN_OPTIONS=detect_leaks=0`. Wallet 0.3.33's ARM image, stack, SRAM, and
physical install block are unchanged. This cleanup reaches descendants
that remain in the RPC child's process group; a descendant that creates a
new session or process group can escape it. The local RPC executable and
node remain external trust inputs. This test does not identify or resolve
the prior physical Blue startup freeze.
