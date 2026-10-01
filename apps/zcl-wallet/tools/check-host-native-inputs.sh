#!/usr/bin/env bash
# Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
# Linux host regression: changed native code reruns JVM tests; unchanged code
# remains reusable. All builds/configuration changes stay in a fresh snapshot.
set -euo pipefail
umask 077
if [[ $# != 1 || $1 != /* ]]; then
    echo 'Usage: check-host-native-inputs.sh /absolute/new-output-directory' >&2
    exit 2
fi
: "${ANDROID_HOME:?Set ANDROID_HOME to the installed SDK}"
wallet_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
repo_root=$(CDPATH= cd -- "$wallet_root/../.." && pwd)
mkdir -- "$1" # Refuse existing files/directories; never replace previous work.
output=$(CDPATH= cd -- "$1" && pwd)
mkdir "$output/source"
# Read current tracked bytes, including the candidate Gradle change. Exclude
# ignored builds, local SDK settings, wallets and operational data.
(
    cd "$repo_root"
    git ls-files -z -- apps/zcl-wallet vendor/android-mbedtls vendor/android-bip39 \
        vendor/android-secp256k1 vendor/android-qrcodegen vendor/android-quirc \
        vendor/android-blake2 contexts/commons/packages/zjsonp contexts/commons/packages/zutf8 |
        tar --null -T - -cf -
) | tar -xf - -C "$output/source"
cd "$output/source/apps/zcl-wallet"

run_tests() {
    timeout 300 ./gradlew --offline --no-daemon --no-build-cache --max-workers=4 \
        --console=plain --info :wallet-core:test > "$output/$1.log" 2>&1
}

require_executed() {
    grep -q '^> Task :wallet-core:test$' "$output/$1.log"
    grep -q 'Gradle Test Executor .* started executing tests' "$output/$1.log"
    # Require a nonempty suite with no failures, errors or skips. Preserve the
    # reports from each execution rather than reusing the last XML as evidence.
    cp -R wallet-core/build/test-results/test "$output/$1-results"
    awk '/<testsuite / {
        for (i=1; i<=NF; ++i) {
            value=$i; gsub(/[^0-9]/, "", value)
            if ($i ~ /^tests=/) tests+=value
            if ($i ~ /^(failures|errors|skipped)=/) refused+=value
        }
    } END { exit !(tests>0 && refused==0) }' "$output/$1-results"/TEST-*.xml
}

library_hashes() {
    sha256sum native/build/jni/libzclwallet_jni.so \
        native/build/jni/libzclwallet_secret_fixture.so
}

cmake -S native -B native/build/jni -DZCL_JNI=ON -DZCL_TLS_REVIEW=OFF \
    -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_FLAGS_DEBUG=-g > "$output/configure.log" 2>&1
run_tests initial
require_executed initial
library_hashes > "$output/initial.sha256"

# A legitimate compiler input changes library bytes without changing any
# declared source file. Test-result reuse must observe the rebuilt artifacts.
cmake -S native -B native/build/jni '-DCMAKE_C_FLAGS_DEBUG=-O2 -g' \
    > "$output/reconfigure.log" 2>&1
run_tests changed
library_hashes > "$output/changed.sha256"
if cmp -s "$output/initial.sha256" "$output/changed.sha256"; then
    echo 'Native bytes did not change: regression control is inconclusive' >&2
    exit 1
fi
require_executed changed

run_tests unchanged
grep -q '^> Task :wallet-core:test UP-TO-DATE$' "$output/unchanged.log"
sha256sum -c "$output/changed.sha256"
echo 'Host JVM input regression passed: changed native bytes rerun tests; unchanged bytes reuse results.'
