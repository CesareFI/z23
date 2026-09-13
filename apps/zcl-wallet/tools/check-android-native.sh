#!/usr/bin/env bash
# Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
set -euo pipefail
if [[ $# != 5 ]]; then
    echo 'Usage: check-android-native.sh <zipalign> <llvm-readelf> <debug.apk> <release.apk> <report-directory>' >&2
    exit 2
fi
zipalign=$1
readelf=$2
debug_apk=$3
release_apk=$4
report=$5
mkdir -p "$report"

fail() { printf 'Native APK check: %s\n' "$*" >&2; exit 1; }

check_library() {
    local apk=$1 label=$2 abi=$3
    local entry="lib/$abi/libzclwallet_jni.so"
    local output="$report/$label-$abi"
    [[ $(awk -v entry="$entry" '$0 == entry { n++ } END { print n+0 }' "$report/$label-entries.txt") == 1 ]] ||
        fail "$label must contain exactly one $entry"
    unzip -p "$apk" "$entry" > "$output.so"
    "$readelf" --program-headers --wide "$output.so" > "$output-headers.txt"
    awk '$1 == "LOAD" { print $2, $3, $NF }' "$output-headers.txt" > "$output-loads.txt"
    local offset address alignment extra count=0
    local hex='^0x[0-9a-fA-F]{1,16}$'
    while read -r offset address alignment extra; do
        [[ -z "$extra" && "$offset" =~ $hex && "$address" =~ $hex && "$alignment" =~ $hex ]] ||
            fail "$label/$abi has an unrecognized LOAD header"
        # Validated hex only. The low-bit comparison works for all ELF64
        # addresses, without subtracting possibly overflowing 64-bit values.
        (( alignment >= 16384 && (alignment & (alignment - 1)) == 0 &&
           (offset & 16383) == (address & 16383) )) ||
            fail "$label/$abi has a LOAD segment incompatible with 16 KiB pages"
        count=$((count + 1))
    done < "$output-loads.txt"
    (( count > 0 )) || fail "$label/$abi has no LOAD segments"
}

check_apk() {
    local apk=$1 label=$2
    "$zipalign" -c -P 16 4 "$apk" > "$report/$label-zipalign.txt" 2>&1 ||
        fail "$label APK is not aligned for 16 KiB native loading"
    unzip -Z1 "$apk" > "$report/$label-entries.txt"
    awk '/^lib\/.*\.so$/ {
        if ($0 != "lib/arm64-v8a/libzclwallet_jni.so" &&
            $0 != "lib/x86_64/libzclwallet_jni.so") bad=1
        n++
    } END { exit bad || n != 2 }' "$report/$label-entries.txt" ||
        fail "$label contains an unexpected native library inventory"
    check_library "$apk" "$label" arm64-v8a
    check_library "$apk" "$label" x86_64
}

check_apk "$debug_apk" debug
check_apk "$release_apk" release
sha256sum "$debug_apk" "$release_apk" > "$report/apks.sha256"
echo 'Native APK check passed: both ABIs have 16 KiB-compatible ELF segments and APK alignment.'
