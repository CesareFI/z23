#!/usr/bin/env bash
# Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
set -euo pipefail
wallet_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
repo_root=$(CDPATH= cd -- "$wallet_root/../.." && pwd)
cd "$wallet_root"

clang_bin=${1:-clang-20}
gcc_bin=${2:-gcc}
tls_review=${3:-OFF}
case "$tls_review" in
    OFF) echo 'Scope: enabled wallet core. TLS remains BLOCKED — REQUIRES FURTHER SECURITY REVIEW.' ;;
    ON) echo 'Scope: explicit host TLS security review, including all preserved provider findings.' ;;
    *) echo 'Usage: check-c-safety.sh [clang] [gcc] [OFF|ON]' >&2; exit 2 ;;
esac
analysis_dir=native/build/analysis-active
if [[ "$tls_review" == ON ]]; then analysis_dir=native/build/analysis-tls-review; fi
mkdir -p "$analysis_dir"

if rg -n '\b(strcpy|strcat|sprintf|vsprintf|gets|scanf|sscanf|fscanf|alloca)\s*\(' native/src native/include; then
    echo 'Forbidden unbounded C API in authored core' >&2
    exit 1
fi

(
    cd "$repo_root/vendor/android-mbedtls"
    sha256sum -c SHA256SUMS
)
(
    cd "$repo_root/vendor/android-bip39"
    sha256sum -c SHA256SUMS
)
(
    cd "$repo_root/vendor/android-secp256k1"
    sha256sum -c SHA256SUMS
)
(
    cd "$repo_root/vendor/android-qrcodegen"
    sha256sum -c SHA256SUMS
)
(
    cd "$repo_root/vendor/android-blake2"
    sha256sum -c SHA256SUMS
)
(
    cd native/tests
    sha256sum -c zip243-reference.sha256
    sha256sum -c chain-context-reference.sha256
)
(
    cd "$repo_root/vendor/android-quirc"
    sha256sum -c SHA256SUMS
)
(
    cd "$repo_root"
    sha256sum -c apps/zcl-wallet/native/json-provider.sha256
)

common=(-std=c17 -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion
    -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096
    '-DMBEDTLS_CONFIG_FILE="zcl_mbedtls_config.h"'
    -I native/include -I "$repo_root/vendor/android-mbedtls/include"
    -I "$repo_root/vendor/android-bip39" -I "$repo_root/vendor/android-secp256k1/include"
    -I "$repo_root/vendor/android-qrcodegen" -I "$repo_root/vendor/android-quirc"
    -I "$repo_root/vendor/android-blake2"
    -I "$repo_root/contexts/commons/packages/zjsonp/include"
    -I "$repo_root/contexts/commons/packages/zutf8/include")
javac_path=$(command -v javac)
jdk_root=$(dirname -- "$(dirname -- "$(readlink -f -- "$javac_path")")")
jni_common=("${common[@]}" -I "$jdk_root/include" -I "$jdk_root/include/linux")
if [[ "$tls_review" == ON ]]; then common+=(-DZCL_TLS_REVIEW=1); fi

for source in native/src/*.c; do
    if [[ "$tls_review" == OFF && "$source" == native/src/transport*.c ]]; then continue; fi
    flags=("${common[@]}")
    case "$source" in
        */jni_*) flags=("${jni_common[@]}") ;;
    esac
    name=${source##*/}
    "$clang_bin" --analyze -Xanalyzer -analyzer-werror "${flags[@]}" "$source" -o "$analysis_dir/$name.plist"
    "$gcc_bin" -fanalyzer "${flags[@]}" -c "$source" -o "$analysis_dir/$name.o"
done

# Provider internals participate in static analysis as well as sanitizers.
# Their reviewed upstream compiler warnings are separate from analyzer findings.
"$clang_bin" --analyze -Xanalyzer -analyzer-werror -std=c17 \
    -Wall -Wextra -Wpedantic -Werror "$repo_root/vendor/android-blake2/blake2b-ref.c" \
    -o "$analysis_dir/blake2b-ref.plist"
"$gcc_bin" -fanalyzer -std=c17 -Wall -Wextra -Wpedantic -Werror \
    -c "$repo_root/vendor/android-blake2/blake2b-ref.c" -o "$analysis_dir/blake2b-ref.o"

for unit in quirc identify decode version_db; do
    "$clang_bin" --analyze -Xanalyzer -analyzer-werror -std=c17 \
        -I "$repo_root/vendor/android-quirc" "$repo_root/vendor/android-quirc/$unit.c" \
        -o "$analysis_dir/quirc-$unit.plist"
done

for unit in zjsonp zutf8; do
    "$clang_bin" --analyze -Xanalyzer -analyzer-werror -std=c17 \
        -I "$repo_root/contexts/commons/packages/zjsonp/include" \
        -I "$repo_root/contexts/commons/packages/zutf8/include" \
        "$repo_root/contexts/commons/packages/$unit/src/$unit.c" \
        -o "$analysis_dir/$unit.plist"
done

# Analyze only the enabled provider translation units. The explicit review
# mode retains the full TLS analysis; its unresolved findings are not waived.
for source in "$repo_root"/vendor/android-mbedtls/library/*.c; do
    name=${source##*/}
    if [[ "$tls_review" == OFF ]]; then
        case "$name" in sha256.c|sha512.c|ripemd160.c|platform_util.c) ;; *) continue ;; esac
    fi
    review_flags=()
    if [[ "$tls_review" == ON ]]; then review_flags=(-DZCL_TLS_REVIEW=1); fi
    "$clang_bin" --analyze -Xanalyzer -analyzer-werror -std=c17 \
        "${review_flags[@]}" \
        '-DMBEDTLS_CONFIG_FILE="zcl_mbedtls_config.h"' -I native/include \
        -I "$repo_root/vendor/android-mbedtls/include" "$source" \
        -o "$analysis_dir/mbedtls-$name.plist"
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

# Test and fuzz code owns fixture lifetimes and assertions too. Apply the
# repository's test cap explicitly; the production cap above remains stricter.
ZCL_CYCLOMATIC_ROOT="$wallet_root/native/tests" ZCL_LINT_MODE=FAIL \
    "$repo_root/build/bin/z23-lint" check-cyclomatic-complexity --report > "$analysis_dir/test-complexity.txt"
cat "$analysis_dir/test-complexity.txt"
if ! rg -q '^  M>10: [0-9]+  M>15: 0  M>20: 0$' "$analysis_dir/test-complexity.txt" ||
    ! rg -q 'report: [1-9][0-9]* functions in [1-9][0-9]* files$' "$analysis_dir/test-complexity.txt"; then
    echo 'Test complexity exceeds 15 or no fixture functions were observed' >&2
    exit 1
fi

build_dir=native/build/safety-active
if [[ "$tls_review" == ON ]]; then build_dir=native/build/safety-tls-review; fi
cmake -S native -B "$build_dir" -DCMAKE_C_COMPILER="$clang_bin" -DZCL_SANITIZE=ON \
    "-DZCL_TEST_JNI_INCLUDE_DIRS=$jdk_root/include;$jdk_root/include/linux" \
    -DZCL_TLS_REVIEW="$tls_review" -DCMAKE_BUILD_TYPE=Debug
cmake --build "$build_dir" -j4
ctest --test-dir "$build_dir" --output-on-failure

# Optimized GCC has different inlining/frame and fortified-libc behavior.
# Debug keeps assertions active; a separate directory prevents mixing objects
# with the Clang profile. Retain provider-specific optimization and sanitizers.
gcc_build_dir=native/build/safety-gcc-active
if [[ "$tls_review" == ON ]]; then gcc_build_dir=native/build/safety-gcc-tls-review; fi
cmake -S native -B "$gcc_build_dir" -DCMAKE_C_COMPILER="$gcc_bin" \
    -DCMAKE_BUILD_TYPE=Debug '-DCMAKE_C_FLAGS_DEBUG=-O2 -g' \
    -DZCL_SANITIZE=ON -DZCL_FUZZ=OFF -DZCL_JNI=OFF -DZCL_ORACLE=OFF \
    "-DZCL_TEST_JNI_INCLUDE_DIRS=$jdk_root/include;$jdk_root/include/linux" \
    -DZCL_TLS_REVIEW="$tls_review" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build "$gcc_build_dir" -j4
ctest --test-dir "$gcc_build_dir" --output-on-failure
echo "C safety checks passed for selected scope (TLS review=$tls_review); manual review remains required."
