#!/usr/bin/env bash
# Copyright 2026 Rhett Creighton - Apache License 2.0
# Prove the inclusive byte limit through the real zdiff CLI.
set -euo pipefail
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
ZDIFF=${1:-"$ROOT/build/bin/zdiff-cli-test"}
[[ -x "$ZDIFF" ]] || { echo 'zdiff_cli_selftest: build the CLI first' >&2; exit 1; }
mkdir -p "$ROOT/test-tmp"
tmp=$(mktemp -d "$ROOT/test-tmp/zdiff-cli-selftest.XXXXXX")
trap 'rm -rf -- "$tmp"' EXIT
dd if=/dev/zero bs=4096 count=1023 2>/dev/null | tr '\000' x > "$tmp/short"
dd if=/dev/zero bs=4095 count=1 2>/dev/null | tr '\000' x >> "$tmp/short"
cp "$tmp/short" "$tmp/exact"
printf Y >> "$tmp/exact"
cp "$tmp/exact" "$tmp/long"
printf Z >> "$tmp/long"
cp "$tmp/exact" "$tmp/long-nul"
printf '\000' >> "$tmp/long-nul"
cp "$tmp/exact" "$tmp/long-ff"
printf '\377' >> "$tmp/long-ff"
: > "$tmp/empty"
[[ $(wc -c < "$tmp/short") == 4194303 ]]
[[ $(wc -c < "$tmp/exact") == 4194304 ]]
[[ $(wc -c < "$tmp/long") == 4194305 ]]
cases=0

check() {
    local name=$1 expected_rc=$2 expected_file=$3 rc=0
    shift 3
    "$ZDIFF" "$@" > "$tmp/out" 2> "$tmp/err" || rc=$?
    if [[ $rc != "$expected_rc" ]] || ! cmp -s "$tmp/out" "$expected_file"; then
        printf 'zdiff_cli_selftest: FAIL %s exit=%s expected=%s\n' \
            "$name" "$rc" "$expected_rc" >&2
        cat "$tmp/err" >&2
        exit 1
    fi
    cases=$((cases + 1))
}

check empty 0 "$tmp/empty" "$tmp/empty" "$tmp/empty"
{ printf '  '; cat "$tmp/short"; printf '\n'; } > "$tmp/expected-short"
check below_limit 0 "$tmp/expected-short" "$tmp/short" "$tmp/short"
{ printf '  '; cat "$tmp/exact"; printf '\n'; } > "$tmp/expected-exact"
check exact_limit 0 "$tmp/expected-exact" "$tmp/exact" "$tmp/exact"
{ printf -- '- '; cat "$tmp/exact"; printf '\n+ '; cat "$tmp/short"; printf '\n'; } > "$tmp/expected-diff"
check last_byte 1 "$tmp/expected-diff" "$tmp/exact" "$tmp/short"
check oversized_old 2 "$tmp/empty" "$tmp/long" "$tmp/exact"
check oversized_new 2 "$tmp/empty" "$tmp/exact" "$tmp/long"
check oversized_nul 2 "$tmp/empty" "$tmp/long-nul" "$tmp/exact"
check oversized_ff 2 "$tmp/empty" "$tmp/long-ff" "$tmp/exact"
check missing_old 2 "$tmp/empty" "$tmp/missing" "$tmp/exact"
check missing_new 2 "$tmp/empty" "$tmp/exact" "$tmp/missing"
check directory 2 "$tmp/empty" "$tmp" "$tmp/exact"
check usage 2 "$tmp/empty"
printf 'zdiff_cli_selftest: PASS %s CLI cases\n' "$cases"
