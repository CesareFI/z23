#!/usr/bin/env bash
# Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
# Purpose: Run the two Linux build-fabric identity groups below root's trust
# boundary without making root an eligible toolchain observer.

set -euo pipefail

PATH=/usr/bin:/bin
export PATH
umask 077

readonly TEST_GROUPS=test_build_fabric,test_build_fabric_attach
readonly RUN_UID=65534
readonly RUN_GID=65534
# The measured fixture is about 350 MiB including both copied executables and
# transient group state. Refuse unless at least 2 GiB remains after copying the
# two inputs, leaving more than five times that observed footprint available.
readonly RESERVE_BYTES=$((2 * 1024 * 1024 * 1024))

fail()
{
    printf 'build-fabric-unprivileged: REFUSE — %s\n' "$*" >&2
    exit 2
}

if [ "$#" -ne 2 ]; then
    fail 'usage: build_fabric_unprivileged_acceptance.sh RUNNER VERIFIER'
fi

root="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd -P)" ||
    fail 'cannot resolve repository root'
runner="$(realpath -- "$1")" || fail 'cannot resolve test runner'
verifier="$(realpath -- "$2")" || fail 'cannot resolve development verifier'

case "$runner" in
    "$root"/build/bin/test-fast/epochs/*/test_parallel_fast) ;;
    *) fail 'runner is not an immutable test-fast epoch candidate' ;;
esac
[ "$verifier" = "$root/build/bin/zclassic23-package-verify-dev" ] ||
    fail 'verifier is not the checkout development verifier'
[ -f "$runner" ] && [ -x "$runner" ] ||
    fail 'test runner is not an executable regular file'
[ -f "$verifier" ] && [ -x "$verifier" ] ||
    fail 'development verifier is not an executable regular file'
[ "$(id -u)" -eq 0 ] || fail 'this acceptance route is for a root host only'
[ "$(uname -s)" = Linux ] || fail 'Linux setpriv is required'
[ -x /usr/bin/setpriv ] || fail '/usr/bin/setpriv is unavailable'

input_safe()
{
    local path="$1" found
    found="$(find "$path" -prune -type f -user root \
        ! -perm -002 ! -perm -020 -print)" || return 1
    [ "$found" = "$path" ]
}

input_safe "$runner" || fail 'runner is not root-owned and non-writable by peers'
input_safe "$verifier" ||
    fail 'verifier is not root-owned and non-writable by peers'

available="$(df --output=avail -B1 /var/tmp | awk 'END {print $1}')" ||
    fail 'cannot measure available /var/tmp bytes'
runner_bytes="$(wc -c < "$runner" | awk '{print $1}')" ||
    fail 'cannot size test runner'
verifier_bytes="$(wc -c < "$verifier" | awk '{print $1}')" ||
    fail 'cannot size development verifier'
for byte_count in "$available" "$runner_bytes" "$verifier_bytes"; do
    case "$byte_count" in
        ''|*[!0-9]*) fail 'disk or input size is not a decimal byte count' ;;
    esac
done
required=$((runner_bytes + verifier_bytes + RESERVE_BYTES))
[ "$available" -ge "$required" ] ||
    fail "insufficient /var/tmp space: available=$available required=$required"

mkdir -p -- "$root/test-tmp"
stamp="$(date -u +%Y%m%dT%H%M%SZ)_$$"
evidence="$root/test-tmp/unprivileged_build_fabric_${stamp}.log"
fixture="$(mktemp -d /var/tmp/z23-unprivileged-build-fabric.XXXXXX)" ||
    fail 'cannot create disposable fixture'

cleanup()
{
    case "${fixture:-}" in
        /var/tmp/z23-unprivileged-build-fabric.*)
            if [ -e "$fixture" ] || [ -L "$fixture" ]; then
                find "$fixture" -xdev -depth -delete
            fi
            ;;
        '') ;;
        *) printf 'build-fabric-unprivileged: refusing unexpected cleanup path: %s\n' \
               "$fixture" >&2 ;;
    esac
}
trap cleanup EXIT
trap 'exit 130' HUP INT TERM

mkdir -p -- "$fixture/build/bin" "$fixture/home" "$fixture/tmp" \
    "$fixture/test-tmp" "$fixture/.cache"
install -m 0500 -- "$runner" "$fixture/build/bin/test_parallel_fast"
install -m 0500 -- "$verifier" \
    "$fixture/build/bin/zclassic23-package-verify-dev"
cmp -s -- "$runner" "$fixture/build/bin/test_parallel_fast" ||
    fail 'runner copy changed bytes'
cmp -s -- "$verifier" \
    "$fixture/build/bin/zclassic23-package-verify-dev" ||
    fail 'verifier copy changed bytes'
chown -R "$RUN_UID:$RUN_GID" -- "$fixture"
chmod 0500 -- "$fixture" "$fixture/build" "$fixture/build/bin"
chmod 0700 -- "$fixture/home" "$fixture/tmp" "$fixture/test-tmp" \
    "$fixture/.cache"

{
    printf 'schema=zcl.build_fabric_unprivileged_acceptance.v1\n'
    printf 'groups=%s\nuid=%s\ngid=%s\nreserve_bytes=%s\n' \
        "$TEST_GROUPS" "$RUN_UID" "$RUN_GID" "$RESERVE_BYTES"
    printf 'runner_sha256='
    sha256sum -- "$fixture/build/bin/test_parallel_fast"
    printf 'verifier_sha256='
    sha256sum -- "$fixture/build/bin/zclassic23-package-verify-dev"
} | tee "$evidence"

ulimit -s unlimited
set +e
(
    cd -- "$fixture" || exit 125
    exec /usr/bin/setpriv --reuid="$RUN_UID" --regid="$RUN_GID" \
        --clear-groups --inh-caps=-all --ambient-caps=-all \
        --bounding-set=-all --no-new-privs \
        /usr/bin/env -i HOME="$fixture/home" TMPDIR="$fixture/tmp" \
        XDG_CACHE_HOME="$fixture/.cache" PATH=/usr/bin:/bin LC_ALL=C TZ=UTC \
        "$fixture/build/bin/test_parallel_fast" --exact="$TEST_GROUPS"
) 2>&1 | tee -a "$evidence"
pipeline_status=("${PIPESTATUS[@]}")
set -e

runner_status="${pipeline_status[0]}"
tee_status="${pipeline_status[1]}"
printf 'runner_exit=%s tee_exit=%s evidence=%s\n' \
    "$runner_status" "$tee_status" "$evidence" | tee -a "$evidence"

for log in "$fixture"/test-tmp/test_parallel_*.log; do
    [ -f "$log" ] || continue
    kept="$root/test-tmp/unprivileged_${stamp}_$(basename -- "$log")"
    cp -- "$log" "$kept"
    chmod 0600 -- "$kept"
done

[ "$tee_status" -eq 0 ] || fail 'could not retain the acceptance transcript'
[ "$runner_status" -eq 0 ] || exit "$runner_status"
printf 'build-fabric-unprivileged: PASS — %s\n' "$evidence"
