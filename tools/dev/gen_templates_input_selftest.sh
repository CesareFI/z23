#!/usr/bin/env bash
# Copyright 2026 CesareFI. Licensed under Apache-2.0.
# A missing or unreadable matching input must not publish an incomplete header.
set -euo pipefail

tool="${1:?generator path required}"
tmp="$(mktemp -d "${TMPDIR:-/tmp}/z23-gen-templates-input.XXXXXX")"
trap 'rm -rf "$tmp"' EXIT

fail() {
    echo "gen_templates_input_selftest: FAIL — $1" >&2
    if [ -f "$tmp/run.log" ]; then cat "$tmp/run.log" >&2; fi
    exit 1
}

mkdir "$tmp/templates"
printf 'hello {{name}}\n' > "$tmp/templates/ok.chtml"
output="$tmp/output.h"

if "$tool" "$tmp/missing" "$output" > "$tmp/run.log" 2>&1; then
    fail "missing template directory was accepted"
fi
[ ! -e "$output" ] || fail "missing directory published a header"

dd if=/dev/zero of="$tmp/templates/large.chtml" bs=1024 count=257 2>/dev/null
if "$tool" "$tmp/templates" "$output" > "$tmp/run.log" 2>&1; then
    fail "oversized template was skipped"
fi
[ ! -e "$output" ] || fail "oversized template published a header"

printf 'keep existing header\n' > "$output"
if "$tool" "$tmp/templates" "$output" > "$tmp/run.log" 2>&1; then
    fail "oversized template replaced an existing header"
fi
printf 'keep existing header\n' | cmp -s - "$output" ||
    fail "failed generation changed the existing header"

rm "$tmp/templates/large.chtml"
if "$tool" "$tmp/templates" "$output" "$tmp/missing-css" > "$tmp/run.log" 2>&1; then
    fail "missing explicit CSS directory was accepted"
fi
printf 'keep existing header\n' | cmp -s - "$output" ||
    fail "missing CSS directory changed the existing header"

"$tool" "$tmp/templates" "$output" > "$tmp/run.log" 2>&1 ||
    fail "valid template failed"
grep -Fq 'static const char TMPL_OK[]' "$output" ||
    fail "valid template was not emitted"
grep -Fq '#define TMPL_PARTIAL_COUNT 1' "$output" ||
    fail "valid template was not registered"
echo "gen_templates_input_selftest: PASS"
