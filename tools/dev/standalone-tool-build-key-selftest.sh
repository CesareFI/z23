#!/usr/bin/env bash
# Copyright 2026 Rhett Creighton - Apache License 2.0
# Exercise the real Make rules in a disposable output tree. No node is run.
set -euo pipefail

quick=0
case "${1:-}" in
    --quick) quick=1 ;;
    '') ;;
    *) printf 'usage: %s [--quick]\n' "$0" >&2; exit 2 ;;
esac

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$root"
scratch="$(mktemp -d "${TMPDIR:-/tmp}/zcl-standalone-tool-key.XXXXXX")"
trap 'rm -rf -- "$scratch"' EXIT
output="$scratch/output"

run_make()
{
    # The parent test runner forwards frozen Make identities through MAKEFLAGS.
    # Each probe must derive its own identity from the selected compiler.
    env -u MAKEFLAGS -u GNUMAKEFLAGS -u MFLAGS -u MAKELEVEL \
        make -s --no-print-directory ZCL_USE_CCACHE=0 BUILD_DIR="$output" "$@"
}

expect_query()
{
    local label="$1" expected="$2" actual=0
    shift 2
    run_make "$@" -q || actual=$?
    if [ "$actual" -ne "$expected" ]; then
        printf 'standalone tool build-key: %s expected make -q rc=%s, got %s\n' \
            "$label" "$expected" "$actual" >&2
        exit 1
    fi
}

printf '#!/usr/bin/env bash\nexec cc "$@"\n' > "$scratch/compiler"
chmod +x "$scratch/compiler"

for tool in jsonq sqlq; do
    run_make CC=cc "$tool"
    test -x "$output/bin/$tool"
    expect_query "$tool unchanged" 0 CC=cc "$tool"

    expect_query "$tool compiler changed" 1 CC="$scratch/compiler" "$tool"
    if [ "$quick" -eq 1 ]; then
        continue
    fi
    run_make CC="$scratch/compiler" "$tool"
    expect_query "$tool compiler unchanged" 0 CC="$scratch/compiler" "$tool"
    expect_query "$tool compiler restored" 1 CC=cc "$tool"
    expect_query "$tool flags changed" 1 CC="$scratch/compiler" \
        ZCL_PLATFORM_CPPFLAGS=-DSTANDALONE_TOOL_KEY_SELFTEST=1 "$tool"

    mv "$output/bin/$tool.build-key" "$scratch/$tool.saved-key"
    expect_query "$tool missing key" 1 CC="$scratch/compiler" "$tool"
    mv "$scratch/$tool.saved-key" "$output/bin/$tool.build-key"
    expect_query "$tool restored key" 0 CC="$scratch/compiler" "$tool"

    cp "$output/bin/$tool.build-key" "$scratch/$tool.saved-key"
    printf 'extra\n' >> "$output/bin/$tool.build-key"
    expect_query "$tool corrupt key" 1 CC="$scratch/compiler" "$tool"
    mv "$scratch/$tool.saved-key" "$output/bin/$tool.build-key"
done

printf 'standalone tool build-key selftest PASS\n'
