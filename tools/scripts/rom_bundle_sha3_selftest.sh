#!/usr/bin/env bash
# Copyright 2026 Rhett Creighton - Apache License 2.0
# A digest is not successful until its complete output is written.
set -euo pipefail

tool="${1:-build/bin/rom_bundle_sha3}"
tmp="$(mktemp -d)"
input="$tmp/abc input"
cleanup() {
    if [ -e "$input" ]; then rm -- "$input"; fi
    if [ -e "$tmp/error" ]; then rm -- "$tmp/error"; fi
    rmdir -- "$tmp"
}
trap cleanup EXIT
printf abc > "$input"
expected="3a985da74fe225b2045c172d6bd390bd855f086e3e9d525b46bfe24511431532  $input"
actual="$("$tool" "$input")"
if [ "$actual" != "$expected" ]; then
    echo 'rom_bundle_sha3 selftest: digest/output mismatch' >&2
    exit 1
fi

if [ -c /dev/full ]; then
    if printf x >/dev/full 2>/dev/null; then
        echo 'rom_bundle_sha3 selftest: /dev/full did not reject writes' >&2
        exit 1
    fi
    if "$tool" "$input" >/dev/full 2>"$tmp/error"; then
        echo 'rom_bundle_sha3 selftest: output failure reported success' >&2
        exit 1
    fi
    if ! grep -Fq "write '$input' failed" "$tmp/error"; then
        echo 'rom_bundle_sha3 selftest: output failure lacks context' >&2
        exit 1
    fi
else
    echo 'rom_bundle_sha3 selftest: output-failure control unavailable (/dev/full absent)' >&2
fi

echo 'rom_bundle_sha3 selftest: PASS'
