#!/usr/bin/env bash
# Copyright 2026 Rhett Creighton - Apache License 2.0
# Exercise unchanged and changed output publication on a tiny source tree.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BIN="${1:-$ROOT/build/bin/gen_capability_inventory}"
case "$(uname -s)" in
    MINGW*|MSYS*)
        echo 'check_capability_inventory_noop: Windows retains replacement behavior'
        exit 0 ;;
esac
WORK="$(mktemp -d "${TMPDIR:-/tmp}/zcl-inventory-noop.XXXXXX")"
trap 'rm -rf -- "$WORK"' EXIT

fail()
{
    echo "check_capability_inventory_noop: FAIL: $*" >&2
    exit 1
}

inode()
{
    ls -di "$1" | awk '{print $1}'
}

run_generator()
{
    "$BIN" "$WORK/out.jsonl" "$WORK/source" >"$WORK/log" 2>&1 || {
        cat "$WORK/log" >&2
        fail 'generator failed'
    }
}

mkdir -p "$WORK/source/include" "$WORK/source/src" "$WORK/source/tools/dev" \
    "$WORK/source/tools/lint"
printf 'int example(void);\n' > "$WORK/source/include/example.h"
printf 'int helper(void) { return 1; }\nint example(void) { return helper(); }\n' > \
    "$WORK/source/src/example.c"
printf 'ZCL_TEST_GROUP(example)\n' > \
    "$WORK/source/tools/dev/test_group_catalog.def"
cat > "$WORK/source/tools/lint/arm_symbol_single_baseline.txt" <<'HEADER'
# z23-generated-artifact: zcl.generated_artifact.v1
# artifact-id: zcl.arm_symbol_single_baseline.v1
# asserts: multi_arm_definition(path,symbol)
# generated-by: tools/lint/check_arm_symbol_single.sh
# regenerate: ZCL_LINT_MODE=UPDATE tools/lint/check_arm_symbol_single.sh
HEADER

run_generator
initial_inode="$(inode "$WORK/out.jsonl")"
cp "$WORK/out.jsonl" "$WORK/expected"
run_generator
[ "$(inode "$WORK/out.jsonl")" = "$initial_inode" ] ||
    fail 'identical output replaced its inode'
cmp -s "$WORK/expected" "$WORK/out.jsonl" || fail 'identical output changed'

printf '/* changed input */\n' >> "$WORK/source/include/example.h"
run_generator
[ "$(inode "$WORK/out.jsonl")" != "$initial_inode" ] ||
    fail 'changed source did not publish a new output'
cmp -s "$WORK/expected" "$WORK/out.jsonl" &&
    fail 'changed source produced the old inventory'
cp "$WORK/out.jsonl" "$WORK/expected"

printf 'corrupt\n' >> "$WORK/out.jsonl"
run_generator
cmp -s "$WORK/expected" "$WORK/out.jsonl" ||
    fail 'corrupt output was not repaired'

cp "$WORK/expected" "$WORK/backing"
rm "$WORK/out.jsonl"
ln -s "$WORK/backing" "$WORK/out.jsonl"
run_generator
[ ! -L "$WORK/out.jsonl" ] || fail 'symlink destination was retained'
cmp -s "$WORK/expected" "$WORK/out.jsonl" ||
    fail 'symlink replacement changed output'
cmp -s "$WORK/expected" "$WORK/backing" ||
    fail 'symlink target was modified'

ln "$WORK/out.jsonl" "$WORK/linked"
run_generator
[ ! "$WORK/out.jsonl" -ef "$WORK/linked" ] ||
    fail 'multiply linked destination was retained'
cmp -s "$WORK/expected" "$WORK/out.jsonl" ||
    fail 'hard-link replacement changed output'

rm "$WORK/out.jsonl"
mkfifo "$WORK/out.jsonl"
"$BIN" "$WORK/out.jsonl" "$WORK/source" >"$WORK/log" 2>&1 &
child=$!
( sleep 5; kill "$child" 2>/dev/null || : ) &
watchdog=$!
if wait "$child"; then
    result=0
else
    result=$?
fi
kill "$watchdog" 2>/dev/null || :
wait "$watchdog" 2>/dev/null || :
[ "$result" -eq 0 ] || fail 'FIFO destination blocked or failed'
[ -f "$WORK/out.jsonl" ] || fail 'FIFO destination was not replaced'
cmp -s "$WORK/expected" "$WORK/out.jsonl" ||
    fail 'FIFO replacement changed output'

echo 'check_capability_inventory_noop: PASS'
