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
echo "inspect_html_count_selftest: PASS"
