#!/usr/bin/env bash
# Copyright 2026 Rhett Creighton - Apache License 2.0
# Exercise the real CLI: oversized selectors must never alias small indices.
set -euo pipefail

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
JSONQ=${1:-"$ROOT/build/bin/jsonq"}
[[ -x "$JSONQ" ]] || { echo "jsonq_selftest: build jsonq first" >&2; exit 1; }
mkdir -p "$ROOT/test-tmp"
tmp=$(mktemp -d "$ROOT/test-tmp/jsonq-selftest.XXXXXX")
trap 'rm -rf -- "$tmp"' EXIT
printf '%s\n' '{"items":[{"id":"first","values":[1,2]},{"id":"second","values":[]}]}' > "$tmp/input"
cases=0

check() {
    local name=$1 expected_rc=$2 expected_out=$3 rc=0
    shift 3
    "$JSONQ" "$@" < "$tmp/input" > "$tmp/out" 2> "$tmp/err" || rc=$?
    printf '%s' "$expected_out" > "$tmp/expected"
    if [[ $rc != "$expected_rc" ]] || ! cmp -s "$tmp/out" "$tmp/expected"; then
        printf 'jsonq_selftest: FAIL %s: exit=%s expected=%s\n' \
            "$name" "$rc" "$expected_rc" >&2
        cat "$tmp/out" "$tmp/err" >&2
        exit 1
    fi
    cases=$((cases + 1))
}

check first 0 $'first\n' get 'items[0].id'
check second 0 $'second\n' get 'items[1].id'
check leading_zeros 0 $'second\n' get '.items[00000000000000000000001].id'
check raw 0 $'"first"\n' raw 'items[0].id'
check type 0 $'string\n' type 'items[0].id'
check has 0 '' has 'items[0].id'
check equals 0 '' eq 'items[0].id' first
check count 0 $'2\n' count 'items[0].values'
check keys 0 $'id\nvalues\n' keys 'items[0]'
check missing 1 '' get 'items[2].id'
check int_max 1 '' get 'items[2147483647].id'
check root 0 $'object\n' type ''
check dotted_root 0 $'object\n' type '.'

key_limit=$(awk 'BEGIN { for (i=0; i<255; i++) printf "k" }')
check key_limit 1 '' get "$key_limit"
check key_oversized 2 '' get "${key_limit}k"
grep -Fq 'jsonq: empty or oversized path segment' "$tmp/err"
depth_limit=$(awk 'BEGIN { for (i=0; i<32; i++) printf "[0]" }')
check depth_limit 1 '' get "$depth_limit"
check depth_oversized 2 '' get "${depth_limit}[0]"
grep -Fq 'jsonq: path too deep' "$tmp/err"
zero_padding=$(awk 'BEGIN { for (i=0; i<4096; i++) printf "0" }')
check long_zero_index 0 $'first\n' get "items[$zero_padding].id"

long_index=$(awk 'BEGIN { for (i=0; i<4096; i++) printf "9" }')
for index in 4294967296 4294967297 2147483648 4294967295 18446744073709551616 "$long_index"; do
    for command in get raw type has count keys; do
        check "overflow_$command" 2 '' "$command" "items[$index]"
        grep -Fq 'jsonq: array index too large' "$tmp/err"
    done
    check overflow_eq 2 '' eq "items[$index].id" first
    grep -Fq 'jsonq: array index too large' "$tmp/err"
done
for path in 'items[]' 'items[-1]' 'items[+1]' 'items[1x]' 'items[1' 'items[0].'; do
    # A trailing dot has historically selected the indexed object; retain it.
    if [[ $path == 'items[0].' ]]; then
        check trailing_dot 0 $'object\n' type "$path"
    else
        check malformed 2 '' get "$path"
    fi
done
printf 'jsonq_selftest: PASS %s CLI cases\n' "$cases"
