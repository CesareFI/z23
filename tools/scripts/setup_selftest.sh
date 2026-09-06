#!/usr/bin/env bash
# Copyright 2026 Rhett Creighton - Apache License 2.0
# Exercise the actual setup recipe with isolated prerequisite failures.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
scratch="$(mktemp -d "${TMPDIR:-/tmp}/z23-setup-selftest.XXXXXX")"
trap 'rm -rf "$scratch"' EXIT HUP INT TERM

fail() {
    printf 'setup selftest: FAIL: %s\n' "$*" >&2
    exit 1
}

# Extract rather than duplicate the production recipe. No repository hooks,
# dependency downloads, compiler, or Git metadata are touched by these fixtures.
awk '
    /^setup:$/ { found=1; print; next }
    found && /^\t/ { print; next }
    found { exit }
' "$ROOT/Makefile" > "$scratch/recipe.mk"
grep -q '^setup:$' "$scratch/recipe.mk" || fail 'setup recipe missing'

run_case() {
    local name="$1" hooks="$2" compdb="$3" sovereign="$4"
    local fixture="$scratch/$name"
    mkdir -p "$fixture"
    cp "$scratch/recipe.mk" "$fixture/Makefile"
    cat >> "$fixture/Makefile" <<'EOF'
install-hooks:
	@touch hooks-ran
	@exit $(HOOKS_EXIT)
compdb:
	@touch compdb-ran
	@exit $(COMPDB_EXIT)
EOF
    case_status=0
    make --no-print-directory -C "$fixture" setup \
        HOOKS_EXIT="$hooks" COMPDB_EXIT="$compdb" \
        ZCL_SOVEREIGN_SOURCE_ROOT="$sovereign" > "$fixture/log" 2>&1 || case_status=$?
}

run_case hook-failure 23 0 ''
[ "$case_status" -ne 0 ] || fail 'hook failure reported success'
[ -f "$scratch/hook-failure/hooks-ran" ] || fail 'hook fixture did not execute'
[ ! -e "$scratch/hook-failure/compdb-ran" ] || fail 'continued after hook failure'
if grep -Eq 'wrote.*\.git/config|setup: done' "$scratch/hook-failure/log"; then
    fail 'hook failure printed a success claim'
fi

run_case success 0 0 ''
[ "$case_status" -eq 0 ] || fail 'successful setup failed'
[ -f "$scratch/success/hooks-ran" ] || fail 'successful setup skipped hooks'
[ -f "$scratch/success/compdb-ran" ] || fail 'successful setup skipped compdb'
grep -q 'setup: done' "$scratch/success/log" || fail 'completion not reported'

run_case sovereign 23 0 "$scratch/source"
[ "$case_status" -eq 0 ] || fail 'sovereign setup failed'
[ ! -e "$scratch/sovereign/hooks-ran" ] || fail 'sovereign setup invoked Git hooks'
[ -f "$scratch/sovereign/compdb-ran" ] || fail 'sovereign setup skipped compdb'

run_case compdb-failure 0 24 ''
[ "$case_status" -ne 0 ] || fail 'compdb failure reported success'
[ -f "$scratch/compdb-failure/compdb-ran" ] || fail 'compdb fixture did not execute'
if grep -q 'setup: done' "$scratch/compdb-failure/log"; then
    fail 'compdb failure printed completion'
fi

printf '%s\n' 'setup selftest: PASS (hook failure, success, sovereign source, compdb failure)'
