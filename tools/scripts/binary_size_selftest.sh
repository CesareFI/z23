#!/usr/bin/env bash
# Copyright 2026 Rhett Creighton - Apache License 2.0
# The documented size helper must measure executable bytes, even through a link.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
work="$(mktemp -d -t zcl-binary-size.XXXXXX)"
trap 'rm -rf -- "$work"' EXIT

printf '0123456789' > "$work/z23"
ln -s z23 "$work/zclassic23"
want=10

[[ "$("$SCRIPT_DIR/binary_size.sh" --bytes "$work/z23")" == "$want" ]] || {
    echo 'binary_size_selftest: regular-file size is wrong' >&2
    exit 1
}
[[ "$("$SCRIPT_DIR/binary_size.sh" --bytes "$work/zclassic23")" == "$want" ]] || {
    echo 'binary_size_selftest: symlink target size is wrong' >&2
    exit 1
}
[[ "$("$SCRIPT_DIR/binary_size.sh" "$work/zclassic23")" == "$want bytes (~0.0 MB)  $work/zclassic23" ]] || {
    echo 'binary_size_selftest: human-readable size is wrong' >&2
    exit 1
}
ln -s missing "$work/broken"
if "$SCRIPT_DIR/binary_size.sh" --bytes "$work/broken" > "$work/out" 2> "$work/err"; then
    echo 'binary_size_selftest: broken link was accepted' >&2
    exit 1
fi
[[ ! -s "$work/out" ]] || {
    echo 'binary_size_selftest: broken link emitted a size' >&2
    exit 1
}

echo 'binary_size_selftest: PASS'
