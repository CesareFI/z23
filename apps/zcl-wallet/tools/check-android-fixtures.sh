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
for variant in debug release; do
    # Inspect the merged APK manifest: a source declaration alone cannot prove
    # that a flavor or dependency preserved the decoder/UI process boundary.
    if ! awk '
        function quoted(line) {
            sub(/^[^=]*="/, "", line); sub(/".*$/, "", line); return line
        }
        function finish() {
            if (kind == "application") applications++
            if (name == "org.zclassic.wallet.ScanDecodeService") {
                decoders++
                if (kind != "service" || exports != 1 || exported != 0 ||
                    processes != 1 || process != ":qrdecode" ||
                    isolates != 1 || isolated != 1 || stops != 1 || stopped != 1) bad=1
            } else if (processes || isolates) bad=1
            if (name == "org.zclassic.wallet.MainActivity") {
                wallets++
                if (kind != "activity" || exports != 1 || exported != 1) bad=1
            }
            if (name == "org.zclassic.wallet.CameraScanActivity") {
                scanners++
                if (kind != "activity" || exports != 1 || exported != 0) bad=1
            }
        }
        /E: / {
            finish()
            kind=$0; sub(/^.*E: /, "", kind); sub(/ .*/, "", kind)
            name=""; process=""; processes=0; exports=0; isolates=0; stops=0
        }
        /A: .*android:name\(/ { name=quoted($0) }
        /A: .*android:process\(/ { processes++; process=quoted($0) }
        /A: .*android:exported\(/ { exports++; exported=($0 ~ /=true$/ ? 1 : ($0 ~ /=false$/ ? 0 : -1)) }
        /A: .*android:isolatedProcess\(/ { isolates++; isolated=($0 ~ /=true$/ ? 1 : 0) }
        /A: .*android:stopWithTask\(/ { stops++; stopped=($0 ~ /=true$/ ? 1 : 0) }
        /A: .*android:(sharedUserId|multiprocess)\(/ { bad=1 }
        END { finish(); exit (bad || applications != 1 || decoders != 1 || wallets != 1 || scanners != 1) }
        ' "$report/$variant-manifest.txt"; then
        echo "Fixture isolation: $variant APK violates the wallet/decoder process boundary" >&2
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
echo 'Fixture isolation passed: private debug hosts, isolated decoder, UI wallet process, test-only assets.'
