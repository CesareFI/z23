#!/usr/bin/env bash
# Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
set -euo pipefail
wallet_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
repo_root=$(CDPATH= cd -- "$wallet_root/../.." && pwd)
cd "$wallet_root"

clang_bin=${1:-clang-20}
gcc_bin=${2:-gcc}
analysis_dir=native/build/analysis
mkdir -p "$analysis_dir"

if rg -n '\b(strcpy|strcat|sprintf|vsprintf|gets|scanf|sscanf|fscanf|alloca)\s*\(' native/src native/include; then
    echo 'Forbidden unbounded C API in authored core' >&2
    exit 1
fi

(
    cd "$repo_root/vendor/android-mbedtls"
    sha256sum -c SHA256SUMS
)

common=(-std=c17 -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion
    -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096
    '-DMBEDTLS_CONFIG_FILE="zcl_mbedtls_config.h"'
    -I native/include -I "$repo_root/vendor/android-mbedtls/include")

for source in native/src/*.c; do
    case "$source" in
        */jni_*) continue ;;
    esac
    name=${source##*/}
    "$clang_bin" --analyze -Xanalyzer -analyzer-werror "${common[@]}" "$source" -o "$analysis_dir/$name.plist"
    "$gcc_bin" -fanalyzer "${common[@]}" -c "$source" -o "$analysis_dir/$name.o"
done

if [[ ! -x "$repo_root/build/bin/z23-lint" ]]; then
    make -C "$repo_root" build/bin/z23-lint
fi
ZCL_CYCLOMATIC_ROOT="$wallet_root/native/src" ZCL_LINT_MODE=FAIL \
    "$repo_root/build/bin/z23-lint" check-cyclomatic-complexity --report > "$analysis_dir/complexity.txt"
cat "$analysis_dir/complexity.txt"
if ! rg -q '^  M>10: 0  M>15: 0  M>20: 0$' "$analysis_dir/complexity.txt"; then
    echo 'Authored core exceeds cyclomatic complexity 10' >&2
    exit 1
fi
if ! rg -q 'report: [1-9][0-9]* functions in [1-9][0-9]* files$' "$analysis_dir/complexity.txt"; then
    echo 'Complexity check did not observe source functions' >&2
    exit 1
fi

cmake -S native -B native/build/safety -DCMAKE_C_COMPILER="$clang_bin" -DZCL_SANITIZE=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build native/build/safety -j4
ctest --test-dir native/build/safety --output-on-failure
echo 'C safety checks passed; manual ownership and threat review is still required before a commit.'
