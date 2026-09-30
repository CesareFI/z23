#!/usr/bin/env bash
# Copyright 2026 Rhett Creighton - Apache License 2.0
# Focused, no-daemon regression for the science acceptance fixture.  All
# writes stay inside a newly-created test-tmp directory; no node datadir is
# accepted from the environment or command line.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
FIXTURE="$REPO_ROOT/build/bin/zcode-science-fixture"
SCRATCH=""

fail() {
    printf 'zcode-science-fixture-selftest: FAIL: %s\n' "$*" >&2
    exit 1
}

cleanup() {
    [ -n "$SCRATCH" ] && [ -d "$SCRATCH" ] || return 0
    case "$SCRATCH" in
        "$REPO_ROOT"/test-tmp/zcl23-science-fixture-*)
            rm -rf -- "$SCRATCH"
            ;;
        *)
            printf 'zcode-science-fixture-selftest: refusing unsafe cleanup: %s\n' \
                "$SCRATCH" >&2
            return 1
            ;;
    esac
}

has_line() {
    local text="$1" expected="$2"
    printf '%s\n' "$text" | grep -Fqx -- "$expected"
}

[ -x "$FIXTURE" ] || fail "$FIXTURE is not executable"
mkdir -p "$REPO_ROOT/test-tmp"
SCRATCH="$(mktemp -d "$REPO_ROOT/test-tmp/zcl23-science-fixture-XXXXXX")" ||
    fail "cannot create scratch directory"
trap cleanup EXIT HUP INT TERM

mkdir -p "$SCRATCH/workspace" "$SCRATCH/datadir"
seed_out="$("$FIXTURE" seed-context "$SCRATCH/workspace" 17)"
study_root="$(printf '%s\n' "$seed_out" | sed -n 's/^STUDY_ROOT=//p')"
[ "${#study_root}" -eq 64 ] || fail "seed-context omitted STUDY_ROOT"
has_line "$("$FIXTURE" cas-has "$SCRATCH/workspace" "$study_root")" \
    'PRESENT=1' || fail "seeded study absent from CAS"
has_line "$("$FIXTURE" cas-has "$SCRATCH/workspace" \
    0000000000000000000000000000000000000000000000000000000000000000)" \
    'PRESENT=0' || fail "absent CAS root reported present"
if "$FIXTURE" cas-has "$SCRATCH/workspace" not-a-root >/dev/null 2>&1; then
    fail "malformed CAS root accepted"
fi

# Public deterministic test vector, never wallet material.
pubkey_out="$("$FIXTURE" pubkey \
    1111111111111111111111111111111111111111111111111111111111111111)"
case "$pubkey_out" in
    PUBKEY=????????????????????????????????????????????????????????????????) ;;
    *) fail "pubkey command returned malformed output" ;;
esac

package_out="$("$FIXTURE" seed-package "$SCRATCH/datadir" 9)"
package_root="$(printf '%s\n' "$package_out" | sed -n 's/^PACKAGE_ROOT=//p')"
[ "${#package_root}" -eq 64 ] || fail "seed-package omitted PACKAGE_ROOT"
verify_out="$("$FIXTURE" verify-package "$SCRATCH/datadir" "$package_root")"
has_line "$verify_out" 'COMPLETE=1' || fail "package is not complete"
has_line "$verify_out" 'ROOT_MATCH=1' || fail "package root did not rederive"
has_line "$verify_out" 'CHUNKS_OK=1' || fail "package chunk verification failed"
has_line "$verify_out" 'CHUNKS_CHECKED=5' || fail "unexpected chunk count"

printf '%s\n' \
    '{"schema":"z23.science_fixture_selftest.v1","verdict":"PASS","cas_present":true,"cas_absent":true,"malformed_root_refused":true,"package_chunks_checked":5}'
