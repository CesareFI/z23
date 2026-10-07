#!/usr/bin/env bash
# Copyright 2026 Rhett Creighton - Apache License 2.0
# Check real CLI output completion, including delayed stdio failures.
set -euo pipefail
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
JSONQ=${1:-"$ROOT/build/bin/jsonq"}
[[ -x "$JSONQ" ]] || { echo 'jsonq_output_selftest: build jsonq first' >&2; exit 1; }
mkdir -p "$ROOT/test-tmp"
tmp=$(mktemp -d "$ROOT/test-tmp/jsonq-output-selftest.XXXXXX")
trap 'rm -rf -- "$tmp"' EXIT
printf '%s' '{"result":{"answer":"ok","array":[1,2]},"error":null}' > "$tmp/input"
printf '{"result":"' > "$tmp/large"
dd if=/dev/zero bs=32768 count=1 2>/dev/null | tr '\000' x >> "$tmp/large"
printf '"}' >> "$tmp/large"
input="$tmp/input"
cases=0

check() {
    local name=$1 expected_rc=$2 expected_out=$3 rc=0
    shift 3
    "$JSONQ" "$@" < "$input" > "$tmp/out" 2> "$tmp/err" || rc=$?
    printf '%s' "$expected_out" > "$tmp/expected"
    if [[ $rc != "$expected_rc" ]] || ! cmp -s "$tmp/out" "$tmp/expected"; then
        printf 'jsonq_output_selftest: FAIL %s exit=%s expected=%s\n' \
            "$name" "$rc" "$expected_rc" >&2
        cat "$tmp/err" >&2
        exit 1
    fi
    cases=$((cases + 1))
}

limited_output() {
    local name=$1 input=$2 expected_rc=$3 expected_error=$4 rc=0 diagnostic
    shift 4
    # Scope EFBIG to this disposable child; no signal death may satisfy a test.
    # stderr uses a pipe, so the limit cannot erase the required diagnostic.
    diagnostic=$(
        (
            trap '' XFSZ
            ulimit -f 0
            exec "$JSONQ" "$@" < "$input" > "$tmp/out"
        ) 2>&1
    ) || rc=$?
    if [[ $rc != "$expected_rc" || $diagnostic != "$expected_error" || -s "$tmp/out" ]]; then
        printf 'jsonq_output_selftest: FAIL %s exit=%s expected=%s; %s\n' \
            "$name" "$rc" "$expected_rc" "$diagnostic" >&2
        exit 1
    fi
    cases=$((cases + 1))
}

check get 0 $'ok\n' get result.answer
check raw 0 $'"ok"\n' raw result.answer
check type 0 $'object\n' type result
check count 0 $'2\n' count result
check keys 0 $'answer\narray\n' keys result
check unwrap 0 $'{"answer":"ok","array":[1,2]}\n' unwrap
check has 0 '' has result.answer
check eq 0 '' eq result.answer ok
check unequal 1 '' eq result.answer other
check missing 1 '' get missing
check usage 2 ''
check unknown 2 '' unknown result
for command in get raw type has count keys; do
    check "${command}_short" 2 '' "$command"
    check "${command}_long" 2 '' "$command" result extra
done
check unwrap_long 2 '' unwrap result
check eq_short 2 '' eq result.answer
check eq_long 2 '' eq result.answer ok extra
input="$tmp/large"
large_text=$(dd if=/dev/zero bs=32768 count=1 2>/dev/null | tr '\000' x)
check large_get 0 "$large_text"$'\n' get result
check large_raw 0 "\"$large_text\""$'\n' raw result
check large_unwrap 0 "$large_text"$'\n' unwrap
for command in get raw type count keys; do
    limited_output "buffered_$command" "$tmp/input" 2 'jsonq: output write failed' "$command" result
done
limited_output buffered_unwrap "$tmp/input" 2 'jsonq: output write failed' unwrap
for command in get raw; do
    limited_output "streaming_$command" "$tmp/large" 2 'jsonq: output write failed' "$command" result
done
limited_output streaming_unwrap "$tmp/large" 2 'jsonq: output write failed' unwrap
limited_output silent_has "$tmp/input" 0 '' has result
limited_output silent_eq "$tmp/input" 0 '' eq result.answer ok
limited_output silent_missing "$tmp/input" 1 '' get missing
printf 'jsonq_output_selftest: PASS %s CLI cases\n' "$cases"
