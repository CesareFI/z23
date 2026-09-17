#!/usr/bin/env bash
# Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
# Validate captured AndroidJUnitRunner output, not device/artifact authenticity.
set -euo pipefail
if [[ $# != 2 ]]; then
    echo 'Usage: check-instrumentation-result.sh <expected-test-count> <captured-log>' >&2
    exit 1
fi
expected=$1
report=$2
case "$expected" in
    ''|0*|*[!0-9]*) echo 'Instrumentation result: invalid expected count' >&2; exit 1 ;;
esac
if [[ ${#expected} -gt 5 ]] || (( expected > 10000 )); then
    echo 'Instrumentation result: expected count exceeds 10000' >&2
    exit 1
fi
if [[ ! -f "$report" ]] || (( $(wc -c < "$report") > 16777216 )); then
    echo 'Instrumentation result: requires a regular log of at most 16 MiB' >&2
    exit 1
fi
LC_ALL=C awk -v expected="$expected" '
    function refuse(reason) {
        print "Instrumentation result: " reason > "/dev/stderr"
        bad=1
        exit 1
    }
    function clear_packet( key) {
        for (key in field) delete field[key]
    }
    function identity() {
        if (field["id"] != "AndroidJUnitRunner" || field["class"] == "" || field["test"] == "")
            refuse("missing test identity")
        if (field["numtests"] != expected || field["numtests"] !~ /^[1-9][0-9]*$/ ||
            field["current"] !~ /^[1-9][0-9]*$/ || field["current"] != passed + 1)
            refuse("test count or order mismatch")
        return field["class"] SUBSEP field["test"]
    }
    { sub(/\r$/, "") }
    finished && $0 !~ /^[[:space:]]*$/ { refuse("data follows terminal result") }
    /^INSTRUMENTATION_STATUS: / {
        if (result) refuse("test data follows result stream")
        value=substr($0, 25)
        separator=index(value, "=")
        if (!separator) refuse("malformed status field")
        key=substr(value, 1, separator-1)
        value=substr(value, separator+1)
        if (key == "class" || key == "test" || key == "id" || key == "numtests" || key == "current") {
            if (key in field) refuse("duplicate status field")
            field[key]=value
        }
        next
    }
    /^INSTRUMENTATION_STATUS_CODE:/ {
        if (result) refuse("test status follows result stream")
        code=substr($0, 30)
        if (code == "1") {
            if (active) refuse("test started before prior completion")
            name=identity()
            if (name in seen) refuse("duplicate test identity")
            active=name
        } else if (code == "0") {
            if (!active || identity() != active) refuse("completion does not match started test")
            seen[active]=1
            active=""
            passed++
        } else if (code != "2") {
            refuse("failed, skipped or unknown test status")
        }
        clear_packet()
        next
    }
    /^INSTRUMENTATION_RESULT:/ {
        if ($0 != "INSTRUMENTATION_RESULT: stream=" || result++ || active || passed != expected)
            refuse("incomplete or failed result stream")
        next
    }
    /^OK \(/ {
        wanted="OK (" expected (expected == 1 ? " test)" : " tests)")
        if (!result || summary++ || $0 != wanted) refuse("summary does not match executed tests")
        next
    }
    /^INSTRUMENTATION_CODE:/ {
        if ($0 != "INSTRUMENTATION_CODE: -1" || !result || summary != 1 || active || passed != expected)
            refuse("missing or unsuccessful terminal result")
        for (key in field) refuse("unterminated status packet")
        finished=1
        next
    }
    /^INSTRUMENTATION_FAILED:|^FAILURES!!!|^FAILURE: / { refuse("runner failure") }
    END {
        if (bad) exit 1
        if (!finished) refuse("missing terminal result")
        print "Instrumentation result: " passed (passed == 1 ? " test" : " tests") " executed and passed; no skips."
    }
' "$report"
