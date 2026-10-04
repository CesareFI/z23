#!/usr/bin/env bash
# Offline argument-boundary regression for the file-service handshake probe.
set -euo pipefail

probe="${1:-build/bin/fs_handshake_probe}"

check_case() {
    local budget="$1" expected_rc="$2" expected_text="$3" actual rc
    if actual="$("$probe" 127.0.0.1 0 "$budget" 2>&1)"; then
        rc=0
    else
        rc=$?
    fi
    if [[ "$rc" != "$expected_rc" || "$actual" != *"$expected_text"* ]]; then
        printf 'fs_handshake_probe args: budget=%q rc=%s, expected rc=%s and %q\n' \
            "$budget" "$rc" "$expected_rc" "$expected_text" >&2
        printf '%s\n' "$actual" >&2
        return 1
    fi
}

check_case 999999999999999999999999999999 2 'invalid budget_ms'
check_case -999999999999999999999999999999 2 'invalid budget_ms'
check_case '' 2 'invalid budget_ms'
check_case 0 2 'invalid budget_ms'
check_case 100ms 2 'invalid budget_ms'
# Port 0 on loopback cannot have a listener. This proves a valid budget reaches
# the connect path while leaving public peers and file services untouched.
check_case 100 3 'connect to 127.0.0.1:0 failed or timed out'
printf 'fs_handshake_probe args: PASS (6 cases)\n'
