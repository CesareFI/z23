#!/bin/sh
# Copyright 2026 Rhett Creighton - Apache License 2.0
# An ignored SIGCHLD must fail the real scanner promptly, never spin forever.
set -eu

root=$(CDPATH= cd "$(dirname "$0")/../.." && pwd)
tmp=$(mktemp -d "${TMPDIR:-/tmp}/z23-fpscan-wait.XXXXXX")
trap 'rm -r "$tmp"' EXIT
cd "$root"

if [ "$#" -eq 0 ]; then
    cc -std=c23 -O2 -Wall -Wextra -Werror -pedantic \
        -D_POSIX_C_SOURCE=200809L \
        -Icognition/modules/fingerprint/include -Iplatform/modules/base/include \
        -o "$tmp/fpscan" tools/fingerprint_scan.c \
        cognition/modules/fingerprint/src/*.c platform/modules/base/src/safe_alloc.c
    scanner="$tmp/fpscan"
else
    scanner=$1
fi
cc -std=c23 -O2 -Wall -Wextra -Werror -pedantic \
    -o "$tmp/ignore-chld" tests/fixtures/fpscan_wait/exec.c
cc -std=c23 -O2 -Wall -Wextra -Werror -pedantic \
    -D_POSIX_C_SOURCE=200809L \
    -Icognition/modules/fingerprint/include -Iplatform/modules/base/include \
    -o "$tmp/wait-unit" tests/fixtures/fpscan_wait/unit.c \
    cognition/modules/fingerprint/src/*.c platform/modules/base/src/safe_alloc.c
"$tmp/wait-unit"

printf '%s\n' 'int add_pair(int a, int b) { return a + b; }' > "$tmp/tiny.c"
printf '%s\n' tiny.c > "$tmp/files.txt"

# Positive control: the fixture selects a real candidate and reaches a fork.
"$scanner" --root="$tmp" --files-from="$tmp/files.txt" \
    --select-only > "$tmp/select.log"
grep -q '^candidates emitted            1$' "$tmp/select.log"

if "$tmp/ignore-chld" "$scanner" --root="$tmp" \
    --work="$tmp/work" --files-from="$tmp/files.txt" --jobs=1 \
    > "$tmp/run.log" 2>&1; then
    echo 'fpscan wait selftest: unavailable child status was accepted' >&2
    exit 1
else
    rc=$?
fi
if [ "$rc" -ne 2 ] || ! grep -q 'fpscan: waitpid failed:' "$tmp/run.log"; then
    echo "fpscan wait selftest: expected wait failure, got rc=$rc" >&2
    tail -12 "$tmp/run.log" >&2
    exit 1
fi
printf '%s\n' 'fpscan wait selftest: PASS'
