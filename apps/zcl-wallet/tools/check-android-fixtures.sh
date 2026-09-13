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
for host in org.zclassic.wallet.WalletDisplayFixtureActivity org.zclassic.wallet.WalletReviewFixtureActivity; do
    if ! awk -v host="$host" '/E:/ { fixture=0; activity=($0 ~ /E: activity/) }
        activity && /android:name/ && index($0, "\"" host "\"") { fixture=1; names++ }
        fixture && /android:exported/ { if ($0 !~ /=false$/) bad=1; found++; fixture=0 }
        END { exit (names != 1 || found != 1 || bad) }' "$report/debug-manifest.txt"; then
        echo "Fixture isolation: expected exactly one nonexported debug host: $host" >&2
        exit 1
    fi
    if rg -F -q "$host" "$report/release-manifest.txt"; then
        echo "Fixture isolation: debug host entered release manifest: $host" >&2
        exit 1
    fi
done
unzip -Z1 "$debug" > "$report/debug-entries.txt"
unzip -Z1 "$release" > "$report/release-entries.txt"
unzip -Z1 "$test_apk" > "$report/test-entries.txt"
if rg -q '^assets/(sync|review)/' "$report/debug-entries.txt" "$report/release-entries.txt"; then
    echo 'Fixture isolation: public response/review assets entered an application APK' >&2
    exit 1
fi
review_count=$(awk '/^assets\/review\/(draft|previous0|previous1)$/ { count++ } END { print count+0 }' "$report/test-entries.txt")
if [[ "$review_count" != 3 ]]; then
    echo 'Fixture isolation: expected three public transactions in the test APK' >&2
    exit 1
fi
count=$(awk '/^assets\/sync\/(mainnet|testnet)-[1-6]\.json$/ { count++ } END { print count+0 }' "$report/test-entries.txt")
if [[ "$count" != 12 ]]; then
    echo 'Fixture isolation: expected twelve public response frames in the test APK' >&2
    exit 1
fi
sha256sum "$debug" "$release" "$test_apk" > "$report/apks.sha256"
echo 'Fixture isolation passed: nonexported debug hosts, absent release hosts, test-only response assets.'
