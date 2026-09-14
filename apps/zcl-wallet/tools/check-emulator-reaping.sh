#!/usr/bin/env bash
# Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
set -euo pipefail
if [[ $# != 2 ]]; then
    echo 'Usage: check-emulator-reaping.sh <qualified-sdk-directory> <new-private-report-directory>' >&2
    exit 2
fi
sdk=$(cd -- "$1" && pwd -P)
umask 077
mkdir -- "$2"
report=$(cd -- "$2" && pwd -P)
wallet_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$wallet_root"
flags=(-std=c17 -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion
    -Wshadow -Wformat=2 -Wvla -Wframe-larger-than=4096)
sanitize=(-O1 -fsanitize=address,undefined -fno-sanitize-recover=all)
adapter=(-Ddlsym=zcl_fixture_dlsym -Ddladdr=zcl_fixture_dladdr)
for source in emulator_reap emulator_reap_preload test_emulator_reap test_emulator_reap_preload fuzz_emulator_reap; do
    definitions=()
    if [[ "$source" == test_emulator_reap_preload ]]; then definitions=("${adapter[@]}"); fi
    clang-20 --analyze -Xanalyzer -analyzer-werror "${flags[@]}" "${definitions[@]}" "tools/$source.c" -o "$report/$source.plist"
    gcc -fanalyzer "${flags[@]}" "${definitions[@]}" -c "tools/$source.c" -o "$report/$source.o"
done
clang-20 "${flags[@]}" "${sanitize[@]}" tools/emulator_reap.c tools/test_emulator_reap.c \
    -pthread -o "$report/unit"
clang-20 "${flags[@]}" "${sanitize[@]}" "${adapter[@]}" tools/emulator_reap.c \
    tools/emulator_reap_preload.c tools/test_emulator_reap_preload.c -ldl -o "$report/adapter"
timeout 30 "$report/unit"
timeout 30 "$report/adapter"
clang-20 "${flags[@]}" -O2 "${adapter[@]}" -DZCL_REAP_MISSING_SYMBOL=1 tools/emulator_reap.c \
    tools/emulator_reap_preload.c tools/test_emulator_reap_preload.c -ldl -o "$report/missing-symbol"
status=0
timeout 10 "$report/missing-symbol" > "$report/missing-symbol.log" 2>&1 || status=$?
[[ "$status" == 125 ]]
rg -q '^Emulator reaping: libc waitpid unavailable$' "$report/missing-symbol.log"

# A real SDK launch exercises ELF constructor ordering and symbol export.
timeout 30 bash tools/run-emulator-reaped.sh "$sdk" "$report/version" -- -version > "$report/version.log" 2>&1
rg -q 'Android emulator version 37.1.11' "$report/version.log"
[[ $(nm -D --defined-only "$report/version/emulator-reap.so" | awk '{print $3}') == waitpid ]]
readelf -W -l -d "$report/version/emulator-reap.so" > "$report/elf.txt"
rg -q GNU_RELRO "$report/elf.txt"
rg -q BIND_NOW "$report/elf.txt"
rg -q 'GNU_STACK.*RW  0x' "$report/elf.txt"

repo_root=$(CDPATH= cd -- "$wallet_root/../.." && pwd)
if [[ ! -x "$repo_root/build/bin/z23-lint" ]]; then make -C "$repo_root" build/bin/z23-lint; fi
mkdir "$report/production" "$report/fixtures"
cp tools/emulator_reap.c tools/emulator_reap_preload.c "$report/production/"
cp tools/test_emulator_reap.c tools/test_emulator_reap_preload.c tools/fuzz_emulator_reap.c "$report/fixtures/"
for scope in production fixtures; do
    ZCL_CYCLOMATIC_ROOT="$report/$scope" ZCL_LINT_MODE=FAIL \
        "$repo_root/build/bin/z23-lint" check-cyclomatic-complexity --report > "$report/$scope-complexity.txt"
    rg -q 'report: [1-9][0-9]* functions in [1-9][0-9]* files$' "$report/$scope-complexity.txt"
done
rg -q '^  M>10: 0  M>15: 0  M>20: 0$' "$report/production-complexity.txt"
rg -q '^  M>10: [0-9]+  M>15: 0  M>20: 0$' "$report/fixtures-complexity.txt"

refuses() {
    local label=$1 expected=$2
    shift 2
    local result=0
    timeout 30 bash tools/run-emulator-reaped.sh "$@" > "$report/$label.log" 2>&1 || result=$?
    [[ "$result" == "$expected" ]]
}
mkdir -p "$report/wrong-sdk/emulator/lib64"
printf '%s\n' 'Unqualified SDK fixture' > "$report/wrong-sdk/emulator/lib64/libandroid-emu-metrics.so"
refuses wrong-sdk 2 "$report/wrong-sdk" "$report/unused" -- -version
[[ ! -e "$report/unused" ]]
refuses existing-report 1 "$sdk" "$report/version" -- -version
refuses space-report 2 "$sdk" "$report/space directory" -- -version
refuses colon-report 2 "$sdk" "$report/colon:directory" -- -version
sha256sum tools/emulator_reap.c tools/emulator_reap.h tools/emulator_reap_preload.c \
    tools/test_emulator_reap.c tools/test_emulator_reap_preload.c \
    tools/fuzz_emulator_reap.c \
    tools/run-emulator-reaped.sh tools/check-emulator-reaping.sh \
    "$report/unit" "$report/adapter" "$report/missing-symbol" > "$report/inputs.sha256"
echo 'Emulator reaping checks passed: C analysis, sanitizers, wait/loader faults, real SDK loading and refusal rails.'
