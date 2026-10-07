#!/usr/bin/env bash
# Copyright 2026 Rhett Creighton - Apache License 2.0
# Exercise the actual Tor dependency setup without compiling or downloading.
set -euo pipefail

repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
scratch="$(mktemp -d "${TMPDIR:-/tmp}/z23-tor-deps.XXXXXX")"
trap 'rm -rf "$scratch"' EXIT HUP INT TERM
fail() { printf 'tor vendor deps selftest: FAIL: %s\n' "$*" >&2; exit 1; }

awk '
    /^# Tor.s TOR_SEARCH_LIBRARY/ { found=1 }
    found && /^mkdir -p "\$TOR_BUILD_DIR"/ { exit }
    found { print }
' "$repo/tools/scripts/build_tor_full.sh" > "$scratch/deps.sh"
[ -s "$scratch/deps.sh" ] || fail 'dependency section missing'

seed_deps() {
    local base="$1" path
    for path in lib/libcrypto.a lib/libssl.a lib/libevent.a lib/libz.a \
        include/openssl/ssl.h include/event2/event.h include/zlib.h; do
        mkdir -p "$base/$(dirname "$path")"
        printf 'fixture\n' > "$base/$path"
    done
}

check_host() (
    ROOT="$scratch/$1"
    HOST_OS="$1"
    VENDOR_TARGET="$2"
    VENDOR_ROOT_DIR="$ROOT/vendor${VENDOR_TARGET:+/cross/$VENDOR_TARGET}"
    configure_opts=()
    seed_deps "$VENDOR_ROOT_DIR"
    source "$scratch/deps.sh"
    [ "${#configure_opts[@]}" -eq 3 ] || fail "$1 did not pin all dependencies"
    [ "${configure_opts[0]}" = "--with-openssl-dir=$VENDOR_ROOT_DIR" ] || fail 'OpenSSL root'
    [ "${configure_opts[1]}" = "--with-libevent-dir=$VENDOR_ROOT_DIR" ] || fail 'libevent root'
    [ "${configure_opts[2]}" = "--with-zlib-dir=$VENDOR_ROOT_DIR" ] || fail 'zlib root'
)

check_host Linux ''
check_host Darwin ''
check_host MINGW64_NT ''
check_host Linux aarch64-linux-gnu
check_host Linux x86_64-w64-mingw32

ROOT="$scratch/missing"
HOST_OS=Linux
VENDOR_TARGET=''
VENDOR_ROOT_DIR="$ROOT/vendor"
mkdir -p "$ROOT/tools/scripts"
printf '#!/bin/sh\nprintf attempted > "%s"\nexit 0\n' "$ROOT/attempted" \
    > "$ROOT/tools/scripts/build_vendor.sh"
chmod +x "$ROOT/tools/scripts/build_vendor.sh"
configure_opts=()
status=0
( source "$scratch/deps.sh" ) > "$scratch/missing.log" 2>&1 || status=$?
[ "$status" -eq 5 ] || fail 'missing dependencies were not refused after build'
[ -s "$ROOT/attempted" ] || fail 'missing dependencies did not trigger vendor build'
grep -q 'still absent:' "$scratch/missing.log" || fail 'missing dependency diagnostic'

printf 'tor vendor deps selftest: PASS (host/cross roots, build attempt, missing refusal)\n'
