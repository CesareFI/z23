#!/bin/sh
# Copyright 2026 Rhett Creighton - Apache License 2.0
# Exercise the real scanner, including its fixed-buffer refusal boundaries.
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
tmp=$(mktemp -d "${TMPDIR:-/tmp}/z23-observability-scan.XXXXXX")
fixture="$tmp/case.c"
log="$tmp/output"
: > "$fixture"
: > "$log"
trap 'rm "$fixture" "$log" "$tmp/check" 2>/dev/null || :; rmdir "$tmp"' EXIT

if [ "$#" -eq 0 ]; then
    cc -std=c23 -O2 -Wall -Wextra -Werror \
        -o "$tmp/check" "$root/tools/check_observability_pairing.c"
    scanner="$tmp/check"
else
    scanner=$1
fi

expect_rc() {
    expected=$1
    if "$scanner" "$fixture" > "$log" 2>&1; then
        actual=0
    else
        actual=$?
    fi
    if [ "$actual" -ne "$expected" ]; then
        printf 'observability selftest: expected rc=%s, got rc=%s\n' \
            "$expected" "$actual" >&2
        cat "$log" >&2
        exit 1
    fi
}

printf '%s\n' '// complete, clean line' > "$fixture"
expect_rc 0
printf '%s\n' 'void f(void) { fprintf(stderr, "unpaired"); }' > "$fixture"
expect_rc 1

# The last allowed line must still be scanned; the next line must not vanish.
awk 'BEGIN { for (i = 1; i < 4095; i++) print "// filler";
             print "void f(void) { fprintf(stderr, \"unpaired\"); }" }' > "$fixture"
expect_rc 1
awk 'BEGIN { for (i = 1; i <= 4096; i++) print "// filler";
             print "void f(void) { fprintf(stderr, \"unpaired\"); }" }' > "$fixture"
expect_rc 2

# A partial line or embedded NUL cannot be treated as a complete C line.
awk 'BEGIN { for (i = 0; i < 4096; i++) printf "x"; print "" }' > "$fixture"
expect_rc 2
printf '/* prefix\000 */\n' > "$fixture"
expect_rc 2

printf '%s\n' 'observability scanner bounds: PASS'
