#!/usr/bin/env bash
# Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
# Refresh stack-tip projections in dependency order using canonical generators.
set -Eeuo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
MODE=refresh
case "${1:-}" in
    '') ;;
    --check) MODE=check ;;
    --selftest) MODE=selftest ;;
    *) echo 'usage: stack_tip_refresh.sh [--check|--selftest]' >&2; exit 2 ;;
esac
[ "$#" -le 1 ] || { echo 'stack-tip-refresh: too many arguments' >&2; exit 2; }
cd "$ROOT"

# Make may also regenerate tracked view headers. Compare against the initial
# working contents, not HEAD, without hashing every unchanged source file.
list_changes() {
    local manifest
    git diff --name-only -z -- || return $?
    git ls-files -z --others --exclude-standard -- || return $?
    # The registry also discovers ignored, untracked package manifests.
    for manifest in contexts/commons/packages/*/zcode-package.json; do
        [ -f "$manifest" ] || continue
        printf '%s\0' "$manifest"
    done
    find engine/composition -type f -name '*.def' -print0
}
content_id() {
    if [ -f "$1" ]; then git hash-object --no-filters -- "$1"
    elif [ ! -e "$1" ]; then printf 'missing\n'
    else echo "stack-tip-refresh: not a regular output: $1" >&2; return 2
    fi
}

selftest() (
    local tmp script="$ROOT/tools/scripts/stack_tip_refresh.sh"
    tmp="$(mktemp -d "${TMPDIR:-/tmp}/zcl-stack-refresh.XXXXXX")"
    trap 'rm -rf "$tmp"' EXIT
    mkdir -p "$tmp/tools/scripts" "$tmp/tools/dev" "$tmp/docs" \
        "$tmp/engine/composition" "$tmp/contexts/commons/packages/fixture"
    cp "$script" "$tmp/tools/scripts/stack_tip_refresh.sh"
    cp "$ROOT/tools/dev/checkout-lock.sh" "$tmp/tools/dev/checkout-lock.sh"
    git -C "$tmp" init -q
    printf 'old\n' > "$tmp/engine/composition/fixture.def"
    printf 'old\n' > "$tmp/docs/CAPABILITY_INVENTORY.jsonl"
    printf 'untouched\n' > "$tmp/unrelated"
    printf 'old\n' > "$tmp/docs/transitive-generated.h"
    git -C "$tmp" add engine docs unrelated
    printf 'already dirty\n' > "$tmp/unrelated"
    printf '/order\n/output\n/fail-*\n/build/\n' > "$tmp/.gitignore"
    cat > "$tmp/tools/scripts/zcode_registry_rederive.sh" <<'FIXTURE'
set -eu
printf 'registry\n' >> order
[ ! -f fail-registry ] || exit 7
if [ "${1:-}" = --check ]; then
    grep -qx fresh engine/composition/fixture.def
else
    printf 'fresh\n' > engine/composition/fixture.def
    printf 'new manifest\n' > contexts/commons/packages/fixture/zcode-package.json
    printf 'fresh\n' > docs/transitive-generated.h
fi
FIXTURE
    cat > "$tmp/Makefile" <<'FIXTURE'
.PHONY: docs-capability-inventory check-capability-inventory-generated check-doc-counts
docs-capability-inventory:
	@echo inventory >> order
	@test ! -f fail-inventory
	@echo fresh > docs/CAPABILITY_INVENTORY.jsonl
check-capability-inventory-generated:
	@echo inventory >> order
	@grep -qx fresh docs/CAPABILITY_INVENTORY.jsonl
check-doc-counts:
	@echo counts >> order
	@test ! -f fail-counts
FIXTURE
    local out="$tmp/output" rc path
    local cli=(bash "$tmp/tools/scripts/stack_tip_refresh.sh")
    # Stale registry refuses before touching inventory; check never rewrites.
    if "${cli[@]}" --check >"$out" 2>&1; then return 1; fi
    grep -qx old "$tmp/engine/composition/fixture.def"
    grep -qx old "$tmp/docs/CAPABILITY_INVENTORY.jsonl"
    [ "$(cat "$tmp/order")" = registry ]
    # Refresh reports its changes, including an untracked generated manifest.
    : > "$tmp/order"
    "${cli[@]}" >"$out" 2>&1
    [ "$(cat "$tmp/order")" = $'registry\ninventory\ncounts' ]
    for path in engine/composition/fixture.def docs/CAPABILITY_INVENTORY.jsonl \
        contexts/commons/packages/fixture/zcode-package.json docs/transitive-generated.h; do
        grep -Fxq "  $path" "$out" || { echo "SELFTEST FAIL: omitted changed file $path" >&2; return 1; }
    done
    if grep -q '  unrelated' "$out"; then return 1; fi
    "${cli[@]}" >"$out" 2>&1
    grep -Fxq 'stack-tip-refresh: changed files: none' "$out"
    "${cli[@]}" --check >"$out" 2>&1
    # Inventory drift alone refuses without rewriting it or running counts.
    printf 'stale\n' > "$tmp/docs/CAPABILITY_INVENTORY.jsonl"
    : > "$tmp/order"
    if "${cli[@]}" --check >"$out" 2>&1; then return 1; fi
    [ "$(cat "$tmp/order")" = $'registry\ninventory' ]
    grep -qx stale "$tmp/docs/CAPABILITY_INVENTORY.jsonl"
    # Each failed step stops the sequence; partial changes remain reported.
    touch "$tmp/fail-inventory"
    printf 'dirty\n' > "$tmp/engine/composition/fixture.def"
    : > "$tmp/order"
    if "${cli[@]}" >"$out" 2>&1; then return 1; fi
    [ "$(cat "$tmp/order")" = $'registry\ninventory' ]
    grep -Fxq '  engine/composition/fixture.def' "$out"
    rm "$tmp/fail-inventory"
    touch "$tmp/fail-registry"
    : > "$tmp/order"
    rc=0
    "${cli[@]}" >"$out" 2>&1 || rc=$?
    [ "$rc" -eq 7 ]
    [ "$(cat "$tmp/order")" = registry ]
    rm "$tmp/fail-registry"
    touch "$tmp/fail-counts"
    : > "$tmp/order"
    if "${cli[@]}" >"$out" 2>&1; then return 1; fi
    [ "$(cat "$tmp/order")" = $'registry\ninventory\ncounts' ]
    if "${cli[@]}" --check >"$out" 2>&1; then return 1; fi
    echo 'stack-tip-refresh: SELFTEST PASS (order, drift, failure, changes, idempotence)'
)

if [ "$MODE" = selftest ]; then selftest; exit 0; fi
# The existing wrapper carries its lock into recursive Make and registry builds.
if [ "${ZCL_CHECKOUT_LOCK_HELD:-0}" != 1 ]; then
    exec bash tools/dev/checkout-lock.sh foreground build/.checkout.lock -- \
        bash "$ROOT/tools/scripts/stack_tip_refresh.sh" "$@"
fi

if [ "$MODE" = check ]; then
    bash tools/scripts/zcode_registry_rederive.sh --check
    make --no-print-directory check-capability-inventory-generated
    make --no-print-directory check-doc-counts
    echo 'stack-tip-refresh: CHECK PASS'
    exit 0
fi

declare -A BEFORE=() SEEN=()
ORIGINAL_CHANGED=()
OUTPUTS="$(mktemp "${TMPDIR:-/tmp}/zcl-stack-outputs.XXXXXX")"
trap 'rm -f "$OUTPUTS"' EXIT
git ls-files --stage -z > "$OUTPUTS"
while IFS= read -r -d '' entry; do
    path="${entry#*$'\t'}"
    read -r mode digest stage <<< "${entry%%$'\t'*}"
    [ "$stage" = 0 ] || { echo 'stack-tip-refresh: unresolved index conflict' >&2; exit 2; }
    if [ "$mode" = 120000 ]; then digest="$(content_id "$path")"; fi
    BEFORE["$path"]="$digest"
done < "$OUTPUTS"
list_changes > "$OUTPUTS"
while IFS= read -r -d '' path; do
    ORIGINAL_CHANGED+=("$path")
    BEFORE["$path"]="$(content_id "$path")"
done < "$OUTPUTS"
report_changes() {
    local rc=$? path after count=0
    trap - EXIT
    if ! list_changes > "$OUTPUTS"; then rc=2; fi
    # The original set includes removed outputs; the new set includes additions.
    for path in "${ORIGINAL_CHANGED[@]}"; do printf '%s\0' "$path" >> "$OUTPUTS"; done
    while IFS= read -r -d '' path; do
        [ "${SEEN[$path]:-0}" = 0 ] || continue
        SEEN["$path"]=1
        if ! after="$(content_id "$path")"; then rc=2; continue; fi
        if [ "$after" != "${BEFORE[$path]:-missing}" ]; then
            if [ "$count" -eq 0 ]; then echo 'stack-tip-refresh: changed files:'; fi
            printf '  %s\n' "$path"
            count=$((count + 1))
        fi
    done < "$OUTPUTS"
    if [ "$count" -eq 0 ]; then echo 'stack-tip-refresh: changed files: none'; fi
    rm -f "$OUTPUTS"
    exit "$rc"
}
trap report_changes EXIT
bash tools/scripts/zcode_registry_rederive.sh
make --no-print-directory docs-capability-inventory
make --no-print-directory check-doc-counts
