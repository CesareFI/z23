#!/usr/bin/env bash
# Copyright 2026 CesareFI. Licensed under Apache-2.0.
# Bound the empty-pattern case so an old non-advancing loop fails promptly.
set -euo pipefail

tool="${1:?inspect_html path required}"
tmp="$(mktemp -d "${TMPDIR:-/tmp}/z23-inspect-html-count.XXXXXX")"
trap 'rm -rf "$tmp"' EXIT
printf '<div>aaaa</div>\n' > "$tmp/input.html"

normal="$("$tool" "$tmp/input.html" --count aa)"
[ "$normal" = 2 ] || {
    echo "inspect_html_count_selftest: normal count failed" >&2
    exit 1
}

if ! empty="$(ulimit -c 0; ulimit -t 2; "$tool" "$tmp/input.html" --count '')"; then
    echo "inspect_html_count_selftest: empty count did not complete" >&2
    exit 1
fi
[ "$empty" = 0 ] || {
    echo "inspect_html_count_selftest: empty count failed" >&2
    exit 1
}

expect_bad_check() {
    local status=0
    "$tool" "$tmp/input.html" "$1" > "$tmp/stdout" 2> "$tmp/stderr" || status=$?
    [ "$status" = 2 ] && [ -s "$tmp/stderr" ] || {
        echo "inspect_html_count_selftest: invalid check was accepted: $1" >&2
        exit 1
    }
}

expect_bad_check --bogus
expect_bad_check --count
expect_bad_check --has
expect_bad_check --no
"$tool" "$tmp/input.html" --has aa >/dev/null
"$tool" "$tmp/input.html" --no missing >/dev/null
echo "inspect_html_count_selftest: PASS"
