# Emulator ADB child reaping

The Linux Android Emulator 37.1.11, build 15917651, can abandon an ADB child
after a command timeout. Its `HostSystem::runCommandPosix` sends `SIGKILL`,
calls `waitpid(child, NULL, WNOHANG)` once, then returns. The child can still be
exiting when that call returns zero. No later wait remains to collect it.

This was reproduced in an isolated API 30 emulator under `strace`: six timed
out ADB commands reached that exact zero-return path. A separate C fixture
missed 256 of 256 immediate nonblocking reaps; a blocking exact-PID wait missed
none. Every child in that C fixture was subsequently reaped.

The [upstream System source](https://android.googlesource.com/platform/external/qemu/+/ae9d18d2b6261179fbd57fffec720a04f7bfb053/android/android-emu-base/android/base/system/System.cpp)
contains the timeout sequence. The corresponding
[ADB caller](https://android.googlesource.com/platform/external/qemu/+/ae9d18d2b6261179fbd57fffec720a04f7bfb053/android/emu/adb/interface/src/android/emulation/control/adb/AdbInterface.cpp)
requests completion and termination on timeout. Source SHA256 values are
`12c8d47741c0084ceb920a126372a5816c56d3e89643129b4b3e8cd69ad23b4a`
and `60c5f208044b12815e78f9749e37786b2412783dc8cb59a6a9c5f96baf32bb73`.
These public sources support the diagnosis; they are not claimed to be the
exact source checkout of the distributed SDK binary.

The installed `libandroid-emu-metrics.so` was independently disassembled. Its
return address immediately after that wait is image offset `0x52b40e`. The
qualified library SHA256 is
`c370e2bf32a932690b87f8b0789984f0154cac76f1cc9b10fefa215ec07a8bac`.
Its GNU build ID is `9834f01672859f47b48ccb691ec592c1b07300db`.

`tools/run-emulator-reaped.sh` builds a small C17 ELF adapter in a new private
report directory and enables it for that emulator launch. It first requires
the exact library hash above. The adapter changes only a positive-child,
NULL-status, exactly-WNOHANG wait at the qualified module and return offset.
It completes that owned child's reap, retrying EINTR. All other polling,
status-returning waits, process groups, options, errors and errno handling
retain their ordinary libc behavior. It adds no signal handler, worker,
allocation, descriptor, PID registry or wildcard child reaper. The SDK files
remain unchanged; neither the adapter nor its fixtures enter an Android APK.

This is a qualified host test workaround, not a portable replacement for libc.
The loader function pointer initializes before SDK workers start and remains
immutable. Forked/executed ADB clients cannot match the SDK module and call
site. A different SDK must be reviewed independently; the launcher refuses its
hash rather than guessing a new offset. Keep the local SDK stable during launch.
An existing LD_PRELOAD/LD_AUDIT setting is refused, and report paths cannot
contain loader list separators. An existing report directory is never reused.

The original timeout function exclusively owns this child and releases its
shared bookkeeping lock before killing/waiting. Completing its wait does not
authorize reaping another component's children. SIGKILL does not impose a
wall-clock deadline on uninterruptible kernel cleanup; keep an outer timeout
on disposable test runs. Stop a healthy owned emulator through `adb emu kill`
and collect its launching terminal's exit status. Allow the SDK's own shutdown
grace period before an outer timeout escalates.

From the wallet directory, with an already configured isolated test AVD and
an existing parent report directory:

```sh
bash tools/check-emulator-reaping.sh "$ANDROID_SDK_ROOT" ../../.cache/android-wallet/reap-check-01
bash tools/run-emulator-reaped.sh "$ANDROID_SDK_ROOT" ../../.cache/android-wallet/emulator-run-01 -- \
  -avd YOUR_ISOLATED_TEST_AVD -no-audio -no-boot-anim -no-snapshot \
  -gpu software -accel on -cores 4 -memory 4096 -port 5560
```

Use the owning AVD registry through `ANDROID_AVD_HOME` when it is separate.
The measured host's `emulator -no-window -accel-check` confirms usable KVM;
choose acceleration according to the host capability. Do not restart or clear
an unrelated emulator to apply this workaround. Existing zombies belong to
their existing parents and cannot be collected by a different shell. At the
start of this investigation, 26 such ADB zombies belonged to the three running
wallet test emulators; those profiles and processes were preserved.

The host gate covers exact site/path bounds, NULL/SIZE_MAX, unsigned address
wrap, ordinary status and option handling, persistent errors, EINTR, loader
lookup failures, errno preservation, missing symbol refusal, 256 actual child
lifecycles including four concurrent owners, real SDK constructor loading,
wrong-SDK refusal, private report creation, Clang/GCC analysis and sanitizers.
Production/test complexity caps remain 10/15. The initial NULL mutant exposed
a fixture length shorter than the module suffix; the test now derives a
sufficient length from the literal. A mutation-only unused-parameter error was
fixed without disabling the warning. Initial logs remain available.

The independent bounded site/path fuzzer completed 71,385,647 executions in
121 seconds without a finding (max_len 4113, timeout 5, RSS limit 512 MiB,
observed 271 MiB). All 16 mutants are detected: 15 intended assertions and one
NULL-access sanitizer interception. The first patched emulator run reaped all
26 forced timeout children; subsequent real-ADB launches and Android checks
are recorded separately, with exact source/binary identities.

The final adapter SHA256 is
`b30d70821d0a3f25b1439b5c2827b712c0afb05d1f24eb20d3b4e83c34157873`.
It reaped all27 completed forced timeouts in `accepted-trace.*` and all12 in
the final `bounded-trace.*` run. Each trace pairs SIGKILL with a blocking
exact-PID wait that returns that PID. The final run booted in76.856 seconds,
had no zombie children while its parent remained alive, and completed console
shutdown with launch exit0, including an in-flight stand-in child. Two earlier
real-ADB cycles also exited normally. The original26 zombies under the three
unrelated running emulators remain unchanged.

Both new regressions are registered in the ordinary native C suite: all85
ASan/UBSan/LSan groups pass in61.37 seconds. Android/JVM, debug/release lint,
scanner fixture and architecture gates pass; APK hashes remain unchanged.
The final host gate passes Clang/GCC analysis with the same mock definitions
used by the test build. The initial GCC fixture conflict came from inherited
libc nonnull attributes on provider implementations; removing only those mock
declarations preserves every runtime assertion and production annotation.

The latest locally signed minified APK
`5d7beeb5c95b435e4f031ecd67a19071a735287934a5afb1aa4b8f7fa9bd30c0`
passed the full public camera fixture on the isolated API 30 emulator in
9.864 seconds: real permission denial, a new request and grant, the specified
public QR review, and released camera resources. The independently packaged
scanner fixture SHA256 is
`7322e86f69b8171b2798abfebdfffb960e429fd1fba828c86681cb81b23968b6`.
The previously qualified release-archive signed-wire executable also passed.
These checks access no operator wallet, private key or real funds and establish
no hardware custody or physical-camera acceptance.

Evidence, traced child PIDs, bounded timeout results and source/artifact hashes
are retained in `.cache/android-wallet/adb-reaping-20260914/`. Two initial
forced-timeout fixtures ended with outer exit137. The first allowed only a
15-second kill grace, shorter than the SDK's logged20 seconds. In the later
`accepted` run, the emulator itself exited0 but its intentionally infinite
stand-in outlived it and kept strace running. The final fixture adds a checked
parent-death SIGKILL, parent-identity recheck and30-second alarm. It is inert,
restricted to the isolated emulator serial, and never connects to ADB or
executes a command. The final bounded run exits0 without escalation. Initial
source/binaries/logs are preserved separately; their outer exits are not
represented as graceful shutdown or wallet failures.
