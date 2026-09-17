#!/usr/bin/env bash
# Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
set -euo pipefail
wallet_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
report=${1:?Usage: test-instrumentation-result.sh <report-directory>}
mkdir -p "$report"
validator="$wallet_root/tools/check-instrumentation-result.sh"
packet() {
    printf 'INSTRUMENTATION_STATUS: class=public.Fixture\n'
    printf 'INSTRUMENTATION_STATUS: current=%s\n' "$1"
    printf 'INSTRUMENTATION_STATUS: id=AndroidJUnitRunner\n'
    printf 'INSTRUMENTATION_STATUS: numtests=2\n'
    printf 'INSTRUMENTATION_STATUS: test=case%s\n' "$1"
    printf 'INSTRUMENTATION_STATUS_CODE: %s\n' "$2"
}
{
    packet 1 1
    printf 'INSTRUMENTATION_STATUS: public_stage=fixture-only\nINSTRUMENTATION_STATUS_CODE: 2\n'
    packet 1 0
    packet 2 1
    packet 2 0
    printf 'INSTRUMENTATION_RESULT: stream=\n\nTime: 0.01\n\nOK (2 tests)\n\nINSTRUMENTATION_CODE: -1\n\n'
} > "$report/control.log"
bash "$validator" 2 "$report/control.log"
sed 's/$/\r/' "$report/control.log" > "$report/crlf.log"
bash "$validator" 2 "$report/crlf.log"
awk '
    /^INSTRUMENTATION_STATUS: class=/ { packet++ }
    packet > 2 && /^INSTRUMENTATION_STATUS/ { next }
    { sub(/numtests=2/, "numtests=1"); sub(/2 tests/, "1 test"); print }
' "$report/control.log" > "$report/single.log"
bash "$validator" 1 "$report/single.log"
refused() {
    local count=$1 input=$2 reason=$3
    if bash "$validator" "$count" "$input" > "$input.result" 2>&1; then
        echo "Instrumentation regression: accepted $input" >&2
        exit 1
    fi
    grep -Fq "$reason" "$input.result"
}
for fault in skip ignored failure error unknown wrong-class wrong-order wrong-total duplicate-test \
    missing-start missing-completion missing-terminal wrong-terminal duplicate-terminal trailing \
    summary-count duplicate-summary missing-result duplicate-result failed-result duplicate-field \
    malformed-field wrong-runner; do
    awk -v fault="$fault" '
        /^INSTRUMENTATION_STATUS_CODE: 0$/ && !changed {
            if (fault == "skip") { $0="INSTRUMENTATION_STATUS_CODE: -4"; changed=1 }
            if (fault == "ignored") { $0="INSTRUMENTATION_STATUS_CODE: -3"; changed=1 }
            if (fault == "failure") { $0="INSTRUMENTATION_STATUS_CODE: -2"; changed=1 }
            if (fault == "error") { $0="INSTRUMENTATION_STATUS_CODE: -1"; changed=1 }
            if (fault == "unknown") { $0="INSTRUMENTATION_STATUS_CODE: 9"; changed=1 }
            if (fault == "missing-completion") { changed=1; next }
        }
        /^INSTRUMENTATION_STATUS_CODE: 1$/ && fault == "missing-start" && !changed { changed=1; next }
        /^INSTRUMENTATION_STATUS: class=/ { classes++; if (fault == "wrong-class" && classes == 2) $0=$0 "Wrong" }
        /^INSTRUMENTATION_STATUS: current=/ && fault == "wrong-order" { sub(/=1$/, "=2") }
        /^INSTRUMENTATION_STATUS: numtests=/ && fault == "wrong-total" { sub(/=2$/, "=3") }
        /^INSTRUMENTATION_STATUS: test=case2$/ && fault == "duplicate-test" { sub(/case2$/, "case1") }
        /^INSTRUMENTATION_STATUS: id=/ && fault == "wrong-runner" { sub(/AndroidJUnitRunner/, "OtherRunner") }
        /^INSTRUMENTATION_STATUS: class=/ && fault == "duplicate-field" && !changed { print; changed=1 }
        /^INSTRUMENTATION_STATUS: class=/ && fault == "malformed-field" { sub(/=/, ":") }
        /^INSTRUMENTATION_RESULT:/ {
            if (fault == "missing-result") next
            if (fault == "duplicate-result") print
            if (fault == "failed-result") $0="INSTRUMENTATION_RESULT: shortMsg=Public fixture failure"
        }
        /^OK \(/ {
            if (fault == "summary-count") sub(/2 tests/, "3 tests")
            if (fault == "duplicate-summary") print
        }
        /^INSTRUMENTATION_CODE:/ {
            if (fault == "missing-terminal") next
            if (fault == "wrong-terminal") sub(/-1$/, "0")
            if (fault == "duplicate-terminal") print
        }
        { print }
        END { if (fault == "trailing") print "unexpected trailing output" }
    ' "$report/control.log" > "$report/$fault.log"
    refused 2 "$report/$fault.log" 'Instrumentation result:'
done
printf 'INSTRUMENTATION_RESULT: stream=\nOK (2 tests)\nINSTRUMENTATION_CODE: -1\n' > "$report/summary-only.log"
refused 2 "$report/summary-only.log" 'incomplete or failed result stream'
: > "$report/empty.log"
refused 2 "$report/empty.log" 'missing terminal result'
head -n 4 "$report/control.log" > "$report/truncated.log"
refused 2 "$report/truncated.log" 'missing terminal result'
cp "$report/control.log" "$report/oversized.log"
dd if=/dev/zero of="$report/oversized.log" bs=1 count=0 seek=16777217 2> "$report/oversized-create.log"
refused 2 "$report/oversized.log" 'at most 16 MiB'
refused 2 "$report/absent.log" 'requires a regular log'
refused 2 "$report" 'requires a regular log'
refused 3 "$report/control.log" 'test count or order mismatch'
for count in 0 01 -1 1.0 arbitrary 10001 999999999999999999999; do
    refused "$count" "$report/control.log" 'Instrumentation result:'
done
echo 'Instrumentation result checker: controls pass; skipped, partial and malformed evidence refuses.'
