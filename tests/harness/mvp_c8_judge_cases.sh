#!/usr/bin/env bash
# Copyright 2026 Rhett Creighton - Apache License 2.0
#
# mvp_c8_judge_cases.sh — executed refusal-path tests for the C8
# genesis-canary judge in tools/mvp_gate.sh.
#
# The judge is inline in a live-node probe, so this harness lifts the real
# judge text out of tools/mvp_gate.sh (verdict reader, identity bind, coarse
# probe and the verdict precedence chain) and runs it against fixture
# verdict files, a faked running-binary identity and a stubbed zclassicd
# height oracle. Nothing here edits or re-implements a judge branch: a
# changed branch in mvp_gate.sh changes what these cases observe.
#
# usage: mvp_c8_judge_cases.sh <repo-root>
# Prints one `CASE <name> VERDICT=<token> FULL=<0|1>` line per case and
# exits 0 only when every token and detail marker matches exactly.
set -uo pipefail

ROOT="${1:?usage: mvp_c8_judge_cases.sh <repo-root>}"
GATE="$ROOT/tools/mvp_gate.sh"
JSONQ="$ROOT/build/bin/jsonq"
[[ -f "$GATE" ]] || { echo "mvp_c8_judge: missing $GATE" >&2; exit 2; }
[[ -x "$JSONQ" ]] || { echo "mvp_c8_judge: missing $JSONQ (make jsonq)" >&2; exit 2; }

mkdir -p "$ROOT/build/scratch"
WORK="$(mktemp -d "$ROOT/build/scratch/mvp-c8-judge.XXXXXX")" || exit 2
trap 'rm -rf "$WORK"' EXIT

# ── lift the real judge segments ──────────────────────────────────
awk '/^CANARY_DIR=/{p=1} /^# Bind release evidence/{p=0} p' "$GATE" > "$WORK/seg_read.sh"
awk '/^json_num\(\) \{/{p=1} p{print} p&&/^\}$/{exit}' "$GATE" > "$WORK/seg_json.sh"
awk '/^G_ID_MATCH=0$/{p=1} p{print} p&&/^fi$/{exit}' "$GATE" > "$WORK/seg_bind.sh"
awk '/^CANARY_LEDGER=/{p=1} /SOAK ACCRUAL CHECK/{exit} p' "$GATE" > "$WORK/seg_verdict.sh"
grep -q "^json_num()" "$WORK/seg_json.sh" &&
grep -q '^canary_read genesis$' "$WORK/seg_read.sh" &&
grep -q 'G_ARTIFACT" == "\$LIVE_ARTIFACT"' "$WORK/seg_bind.sh" &&
grep -q '^set_v 8 "BLOCKED"\|^    set_v 8 "BLOCKED"' "$WORK/seg_verdict.sh" &&
grep -q 'set_v 8 "PASS"' "$WORK/seg_verdict.sh" || {
    echo "mvp_c8_judge: could not lift the C8 judge segments from mvp_gate.sh" >&2
    exit 2
}

SRC_OK="$(printf '1%.0s' {1..64})"
ART_OK="$(printf '2%.0s' {1..64})"
SRC_BAD="$(printf '3%.0s' {1..64})"
ART_BAD="$(printf '4%.0s' {1..64})"
MAX_AGE=604800
FAILURES=0

# run_case <name> <genesis-json|ABSENT> <c23-height> <zd-height> <verdict> <full> <detail-glob>
run_case() {
    local name="$1" json="$2" height="$3" zdh="$4" want_v="$5" want_f="$6" want_d="$7"
    local dir="$WORK/$name"
    mkdir -p "$dir"
    [[ "$json" == ABSENT ]] || printf '%s' "$json" > "$dir/replay_canary_genesis.json"
    local out
    out="$(
        export ZCL_CANARY_VERDICT_DIR="$dir" CANARY_MAX_AGE_S="$MAX_AGE"
        export ZCL_JSONQ="$JSONQ" MVP_REPO_ROOT="$ROOT"
        TIP_GAP_OK=10
        declare -A VERDICT DETAIL FULL
        set_v() { VERDICT[$1]="$2"; DETAIL[$1]="$3"; FULL[$1]="${4:-0}"; }
        zd_rpc() { printf '{"result":%s,"error":null}' "$ZD_H_STUB"; }
        ZD_H_STUB="$zdh"; ZD_RPCPORT=0
        NODE_UP=1; HEIGHT="$height"
        LIVE_SOURCE_ID="$SRC_OK"; LIVE_ARTIFACT="$ART_OK"
        LIVE_ID_DETAIL="running source=$SRC_OK artifact=$ART_OK"
        # shellcheck disable=SC1090
        . "$WORK/seg_json.sh"
        . "$WORK/seg_read.sh"
        . "$WORK/seg_bind.sh"
        . "$WORK/seg_verdict.sh"
        printf 'CASE %s VERDICT=%s FULL=%s\n' "$name" "${VERDICT[8]:-unset}" "${FULL[8]:-unset}"
        printf 'DETAIL %s\n' "${DETAIL[8]:-}"
    )"
    local line detail
    line="$(printf '%s\n' "$out" | grep '^CASE ')"
    detail="$(printf '%s\n' "$out" | sed -n 's/^DETAIL //p')"
    printf '%s\n' "$line"
    if [[ "$line" != "CASE $name VERDICT=$want_v FULL=$want_f" || "$detail" != $want_d ]]; then
        printf 'MISMATCH %s: want VERDICT=%s FULL=%s detail~%s, got detail=%s\n' \
            "$name" "$want_v" "$want_f" "$want_d" "$detail" >&2
        FAILURES=$((FAILURES + 1))
    fi
}

verdict_json() {  # <verdict> <ts> <src> <artifact>
    printf '{"verdict":"%s","ts":%s,"source_id_sha256":"%s","artifact_sha256":"%s"}' "$1" "$2" "$3" "$4"
}

NOW="$(date +%s)"
FRESH=$((NOW - 60))
STALE=$((NOW - MAX_AGE - 3600))

run_case absent ABSENT 1000 1000 BLOCKED 0 \
    '*no fresh canary PASS*genesis=absent age=-1s*'
run_case stale "$(verdict_json PASS "$STALE" "$SRC_OK" "$ART_OK")" 1000 1000 BLOCKED 0 \
    '*no fresh canary PASS*genesis=PASS age=6*'
run_case wrong_source "$(verdict_json PASS "$FRESH" "$SRC_BAD" "$ART_OK")" 1000 1000 BLOCKED 0 \
    '*PASS belongs to different or unreadable bytes*'
run_case wrong_artifact "$(verdict_json PASS "$FRESH" "$SRC_OK" "$ART_BAD")" 1000 1000 BLOCKED 0 \
    '*PASS belongs to different or unreadable bytes*'
run_case verdict_fail "$(verdict_json FAIL "$FRESH" "$SRC_OK" "$ART_OK")" 1000 1000 FAIL 0 \
    'replay canary (genesis) FAIL*'
run_case height_skew "$(verdict_json PASS "$FRESH" "$SRC_OK" "$ART_OK")" 1000 1011 FAIL 0 \
    'height divergence vs zclassicd*'
run_case height_edge "$(verdict_json PASS "$FRESH" "$SRC_OK" "$ART_OK")" 1000 1010 PASS 1 \
    'EXACT parity: replay-canary (genesis) PASS*'
run_case valid "$(verdict_json PASS "$FRESH" "$SRC_OK" "$ART_OK")" 1000 1000 PASS 1 \
    'EXACT parity: replay-canary (genesis) PASS*coarse height MATCH*'

if [[ "$FAILURES" -ne 0 ]]; then
    echo "mvp_c8_judge: $FAILURES case(s) failed" >&2
    exit 1
fi
echo "mvp_c8_judge: ALL CASES OK"
