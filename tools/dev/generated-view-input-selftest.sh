#!/usr/bin/env bash
# Copyright 2026 Rhett Creighton - Apache License 2.0
# Exercise the real CSS view rule with input bytes changed under an old mtime.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$root"
scratch="$(mktemp -d "${TMPDIR:-/tmp}/zcl-view-input.XXXXXX")"
cleanup()
{
    for name in site.css original.css site_css.h site.key; do
        if [ -e "$scratch/$name" ]; then unlink "$scratch/$name"; fi
    done
    rmdir "$scratch"
}
trap cleanup EXIT

source="$scratch/site.css"
header="$scratch/site_css.h"
key="$scratch/site.key"
run_make()
{
    make -s --no-print-directory SITE_CSS_SRC="$source" \
        SITE_CSS_GEN="$header" SITE_CSS_INPUT_KEY_FILE="$key" "$@"
}
expect_query()
{
    local label="$1" expected="$2" actual=0
    run_make -q "$header" >/dev/null 2>&1 || actual=$?
    if [ "$actual" -ne "$expected" ]; then
        printf 'generated view input: %s expected make -q rc=%s, got %s\n' \
            "$label" "$expected" "$actual" >&2
        exit 1
    fi
}
header_digest()
{
    sha256sum < "$header" | awk '{print $1}'
}

printf 'body{color:red}\n' > "$source"
touch -t 202001010000 "$source"
cp -p "$source" "$scratch/original.css"
run_make "$header" >/dev/null
original="$(header_digest)"
expect_query 'settled input' 0

actual=0
run_make VIEW_INPUT_KEY_TOOL=false help >/dev/null 2>&1 || actual=$?
if [ "$actual" -ne 0 ]; then
    echo 'generated view input: help required unrelated view fingerprints' >&2
    exit 1
fi
actual=0
run_make -q VIEW_INPUT_KEY_TOOL=false "$header" >/dev/null 2>&1 || actual=$?
if [ "$actual" -ne 2 ]; then
    echo 'generated view input: view goal skipped fingerprint validation' >&2
    exit 1
fi
actual=0
run_make -q VIEW_INPUT_KEY_TOOL=false help "$header" >/dev/null 2>&1 || actual=$?
if [ "$actual" -ne 2 ]; then
    echo 'generated view input: mixed goal skipped fingerprint validation' >&2
    exit 1
fi

printf 'body{color:blue}\n' > "$source"
touch -r "$scratch/original.css" "$source"
expect_query 'changed bytes with old mtime' 1
run_make "$header" >/dev/null
changed="$(header_digest)"
if [ "$changed" = "$original" ]; then
    echo 'generated view input: changed CSS did not change output' >&2
    exit 1
fi

cp -p "$scratch/original.css" "$source"
expect_query 'restored bytes with old mtime' 1
run_make "$header" >/dev/null
if [ "$(header_digest)" != "$original" ]; then
    echo 'generated view input: restored CSS did not restore output' >&2
    exit 1
fi
expect_query 'restored input settled' 0

actual=0
make -s --no-print-directory -q SITE_CSS_SRC="$scratch/original.css" \
    SITE_CSS_GEN="$header" SITE_CSS_INPUT_KEY_FILE="$key" "$header" \
    >/dev/null 2>&1 || actual=$?
if [ "$actual" -ne 1 ]; then
    echo 'generated view input: input path was not bound' >&2
    exit 1
fi
actual=0
make -s --no-print-directory -q SITE_CSS_SRC="$scratch/missing.css" \
    SITE_CSS_GEN="$header" SITE_CSS_INPUT_KEY_FILE="$key" "$header" \
    >/dev/null 2>&1 || actual=$?
if [ "$actual" -ne 2 ]; then
    echo 'generated view input: missing input did not fail closed' >&2
    exit 1
fi

printf 'extra\n' >> "$key"
expect_query 'corrupt key' 1
printf 'generated view input selftest PASS\n'
