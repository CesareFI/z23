#!/usr/bin/env bash
# Copyright 2026 Rhett Creighton - Apache License 2.0
# Real read-only CLI checks, including buffered and already-failed output.
set -euo pipefail
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
SQLQ=${1:-"$ROOT/build/bin/sqlq"}
[[ -x "$SQLQ" ]] || { echo "sqlq_selftest: build sqlq first" >&2; exit 1; }
mkdir -p "$ROOT/test-tmp"
tmp=$(mktemp -d "$ROOT/test-tmp/sqlq-selftest.XXXXXX")
trap 'rm -rf -- "$tmp"' EXIT
: > "$tmp/fixture.db"
chmod 0444 "$tmp/fixture.db"
cases=0

check() {
    local name=$1 expected_rc=$2 expected_out=$3 rc=0
    shift 3
    "$SQLQ" "$@" > "$tmp/out" 2> "$tmp/err" || rc=$?
    printf '%s' "$expected_out" > "$tmp/expected"
    if [[ $rc != "$expected_rc" ]] || ! cmp -s "$tmp/out" "$tmp/expected"; then
        printf 'sqlq_selftest: FAIL %s: exit=%s expected=%s\n' \
            "$name" "$rc" "$expected_rc" >&2
        cat "$tmp/out" "$tmp/err" >&2
        exit 1
    fi
    cases=$((cases + 1))
}

output_failure() {
    local name=$1 query=$2 rc=0 diagnostic
    # Only this disposable child receives a zero file-size limit. Ignoring
    # SIGXFSZ lets stdio report EFBIG, so an unchecked write cannot pass merely
    # because a signal terminated the process. stderr travels over a pipe.
    diagnostic=$(
        (
            trap '' XFSZ
            ulimit -f 0
            exec "$SQLQ" "$tmp/fixture.db" "$query" > "$tmp/out"
        ) 2>&1
    ) || rc=$?
    if [[ $rc != 1 || $diagnostic != 'sqlq: output write failed' || -s "$tmp/out" ]]; then
        printf 'sqlq_selftest: FAIL %s: exit=%s expected=1; %s\n' \
            "$name" "$rc" "$diagnostic" >&2
        exit 1
    fi
    cases=$((cases + 1))
}

check columns 0 $'text\t42\t3.5\tNULL\t00abff\n' "$tmp/fixture.db" \
    "SELECT 'text',42,3.5,NULL,X'00abff'"
check empty_blob 0 $'\n' "$tmp/fixture.db" "SELECT X''"
check no_rows 0 '' "$tmp/fixture.db" 'SELECT 1 WHERE 0'
check prepare_error 1 '' "$tmp/fixture.db" 'SELECT * FROM missing_table'
check read_only 1 '' "$tmp/fixture.db" 'DELETE FROM missing_table'
check missing_database 1 '' "$tmp/missing.db" 'SELECT 1'
[[ ! -e "$tmp/missing.db" && ! -s "$tmp/fixture.db" ]]
check usage 1 ''
output_failure buffered 'SELECT 1'
output_failure streaming 'SELECT hex(zeroblob(65536))'
[[ ! -s "$tmp/fixture.db" ]]
printf 'sqlq_selftest: PASS %s CLI cases\n' "$cases"
