#!/usr/bin/env bash
# Copyright 2026 Rhett Creighton - Apache License 2.0
# Exercise the real lintc object graph: a query must detect changed compiler
# bytes and flags, while a rebuilt configuration must be a true no-op.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd -P)"
cd "$ROOT"
mkdir -p build
target=build/lintc-obj/lib.o
tmp="$(mktemp -d build/lintc-config-test.XXXXXX)"
trap 'rm -rf -- "$tmp"' EXIT

expect_query()
{
    local expected="$1" label="$2" rc=0
    shift 2
    make --no-print-directory -s -q "$target" "$@" >"$tmp/query.log" 2>&1 || rc=$?
    if [ "$rc" -ne "$expected" ]; then
        printf 'lintc build-config: %s: expected make -q=%s, got %s\n' \
            "$label" "$expected" "$rc" >&2
        cat "$tmp/query.log" >&2
        return 1
    fi
}

wrapper="$ROOT/$tmp/compiler"
printf '#!/bin/sh\nexec cc "$@"\n' > "$wrapper"
chmod 755 "$wrapper"

make --no-print-directory -s "$target" CC=cc
expect_query 0 'unchanged cc' CC=cc
expect_query 1 'different compiler command' "CC=$wrapper"
expect_query 1 'skip override cannot hide compiler change' \
    "CC=$wrapper" LINTC_CONFIG_SKIP=1

make --no-print-directory -s "$target" "CC=$wrapper"
expect_query 0 'unchanged wrapper' "CC=$wrapper"
printf '# compiler byte mutation\n' >> "$wrapper"
expect_query 1 'changed compiler bytes' "CC=$wrapper"

make --no-print-directory -s "$target" "CC=$wrapper"
expect_query 0 'rebuilt wrapper' "CC=$wrapper"
expect_query 1 'switch back to cc' CC=cc

make --no-print-directory -s "$target" CC=cc
expect_query 0 'rebuilt cc' CC=cc
expect_query 1 'changed compile flags' CC=cc LINTC_CFLAGS=-DLINTC_CONFIG_TEST=1
printf 'lintc build-config: PASS (compiler command, bytes, flags, switch-back, no-op controls)\n'
