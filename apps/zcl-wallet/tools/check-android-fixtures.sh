#!/usr/bin/env bash
# Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
set -euo pipefail
wallet_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
aapt=${1:?Usage: check-android-fixtures.sh <sdk-aapt2> <report-directory>}
report=${2:?A report directory is required}
mkdir -p "$report"
debug="$wallet_root/android-app/build/outputs/apk/debug/android-app-debug.apk"
release="$wallet_root/android-app/build/outputs/apk/release/android-app-release-unsigned.apk"
test_apk="$wallet_root/android-app/build/outputs/apk/androidTest/debug/android-app-debug-androidTest.apk"

"$aapt" dump xmltree --file AndroidManifest.xml "$debug" > "$report/debug-manifest.txt"
"$aapt" dump xmltree --file AndroidManifest.xml "$release" > "$report/release-manifest.txt"
if ! awk '/E: activity/ { fixture=0 }
    /android:name.*WalletDisplayFixtureActivity/ { fixture=1 }
    fixture && /android:exported/ { if ($0 !~ /=false$/) bad=1; found++; fixture=0 }
    END { exit (found != 1 || bad) }' "$report/debug-manifest.txt"; then
    echo 'Fixture isolation: expected exactly one nonexported debug host' >&2
    exit 1
fi
if rg -q 'WalletDisplayFixtureActivity' "$report/release-manifest.txt"; then
    echo 'Fixture isolation: debug host entered release manifest' >&2
    exit 1
fi
unzip -Z1 "$debug" > "$report/debug-entries.txt"
unzip -Z1 "$release" > "$report/release-entries.txt"
unzip -Z1 "$test_apk" > "$report/test-entries.txt"
if rg -q '^assets/sync/' "$report/debug-entries.txt" "$report/release-entries.txt"; then
    echo 'Fixture isolation: sync response assets entered an application APK' >&2
    exit 1
fi
count=$(awk '/^assets\/sync\/(mainnet|testnet)-[1-6]\.json$/ { count++ } END { print count+0 }' "$report/test-entries.txt")
if [[ "$count" != 12 ]]; then
    echo 'Fixture isolation: expected twelve public response frames in the test APK' >&2
    exit 1
fi
sha256sum "$debug" "$release" "$test_apk" > "$report/apks.sha256"
echo 'Fixture isolation passed: nonexported debug host, absent release host, test-only response assets.'
