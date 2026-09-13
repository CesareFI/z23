#!/usr/bin/env bash
# Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
set -euo pipefail
adb=${1:?Usage: check-process-relaunch.sh <sdk-adb> <emulator-serial> <new-report-directory> [balance|history|review]}
serial=${2:?An explicit emulator serial is required}
report=${3:?A new report directory is required}
profile=${4:-balance}
fixture=org.zclassic.wallet.BalanceProcessInstrumentedTest
relaunch_method=newProcessStartsWithoutBalanceOrReplay
case "$profile" in
    balance|history) ;;
    review) fixture=org.zclassic.wallet.ReviewProcessInstrumentedTest
        relaunch_method=newProcessStartsWithoutReviewOrReplay ;;
    *) echo 'Process fixture profile must be balance, history or review' >&2; exit 1 ;;
esac
if [[ ! "$serial" =~ ^emulator-[0-9]+$ ]]; then
    echo 'Process fixture requires an explicit emulator serial' >&2
    exit 1
fi
# Keep every run's preparation/termination records; never erase a previous run.
mkdir "$report"
package=org.zclassic.wallet.dev
runner=org.zclassic.wallet.dev.test/androidx.test.runner.AndroidJUnitRunner
timeout 180 "$adb" -s "$serial" shell am instrument -w -r -e processKillFixture yes \
    -e reportProfile "$profile" \
    -e class "$fixture#preparePublicReportForTermination" "$runner" > "$report/prepare.log" 2>&1 &
preparation_pid=$!
trap 'if kill -0 "$preparation_pid" 2>/dev/null; then kill "$preparation_pid"; fi' EXIT
ready=false
for ((attempt=0; attempt<150; ++attempt)); do
    if rg -q '^INSTRUMENTATION_STATUS: zcl_process_fixture=ready$' "$report/prepare.log" &&
        rg -q '^INSTRUMENTATION_STATUS_CODE: 2$' "$report/prepare.log"; then
        ready=true
        break
    fi
    if ! kill -0 "$preparation_pid" 2>/dev/null; then break; fi
    sleep 1
done
if [[ "$ready" != true ]]; then
    echo 'Process fixture never reached verified display readiness' >&2
    exit 1
fi
if ! rg -q "^INSTRUMENTATION_STATUS: zcl_fixture_profile=$profile$" "$report/prepare.log"; then
    echo 'Prepared display profile does not match the requested fixture' >&2
    exit 1
fi
fixture_pid=$(sed -n 's/^INSTRUMENTATION_STATUS: zcl_fixture_pid=\([0-9][0-9]*\)$/\1/p' "$report/prepare.log")
if [[ ! "$fixture_pid" =~ ^[0-9]+$ ]]; then
    echo 'Process fixture did not provide exactly one valid PID' >&2
    exit 1
fi
timeout 30 "$adb" -s "$serial" shell pidof "$package" > "$report/before-pid.txt"
observed_pid=$(tr -d '\r\n' < "$report/before-pid.txt")
if [[ "$observed_pid" != "$fixture_pid" ]]; then
    echo 'Prepared process identity changed before termination' >&2
    exit 1
fi
timeout 30 "$adb" -s "$serial" shell am force-stop "$package" > "$report/termination.log" 2>&1
timeout 30 "$adb" -s "$serial" shell 'if pidof org.zclassic.wallet.dev; then echo RUNNING; else echo STOPPED; fi' > "$report/after-pid.txt"
if [[ "$(tr -d '\r\n' < "$report/after-pid.txt")" != STOPPED ]]; then
    echo 'Prepared app process remained alive after termination' >&2
    exit 1
fi
if wait "$preparation_pid"; then preparation_status=0; else preparation_status=$?; fi
printf '%s\n' "$preparation_status" > "$report/prepare-host-status.txt"
trap - EXIT
timeout 180 "$adb" -s "$serial" shell am instrument -w -r -e processKillFixture yes \
    -e reportProfile "$profile" \
    -e previousPid "$fixture_pid" -e class "$fixture#$relaunch_method" \
    "$runner" > "$report/relaunch.log" 2>&1
rg -q '^OK \(1 test\)' "$report/relaunch.log"
echo 'Process relaunch passed: verified public report, terminated original PID, empty display in a new process.'
