#!/usr/bin/env bash
# Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
set -euo pipefail
wallet_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
export ZCL_FIXTURE_TEST_AAPT=${1:?Usage: test-android-fixtures.sh <sdk-aapt2> <report-directory>}
export ZCL_FIXTURE_TEST_UNZIP
ZCL_FIXTURE_TEST_UNZIP=$(command -v unzip)
report=${2:?A report directory is required}
mkdir -p "$report"
# Only aapt2 output is fault-injected; actual built APKs are never modified.
cat > "$report/fault-aapt" <<'FIXTURE'
#!/usr/bin/env bash
set -euo pipefail
case "$ZCL_FIXTURE_TEST_FAULT:$5" in
    exported:*/debug/android-app-debug.apk|missing-exported:*/debug/android-app-debug.apk|missing:*/debug/android-app-debug.apk)
        "$ZCL_FIXTURE_TEST_AAPT" "$@" | awk -v host="$ZCL_FIXTURE_TEST_HOST" -v fault="$ZCL_FIXTURE_TEST_FAULT" '
            /E:/ { fixture=0 }
            /android:name/ && index($0, "\"" host "\"") { fixture=1; if (fault == "missing") next }
            fixture && /android:exported/ {
                if (fault == "missing-exported") next
                if (fault == "exported") sub(/=false$/, "=true")
            }
            { print }' ;;
    release:*/release/android-app-release-unsigned.apk)
        printf '%s\n' 'E: activity' "A: android:name=\"$ZCL_FIXTURE_TEST_HOST\"" ;;
    *) "$ZCL_FIXTURE_TEST_AAPT" "$@" ;;
esac
FIXTURE
chmod u+x "$report/fault-aapt"
bash "$wallet_root/tools/check-android-fixtures.sh" "$ZCL_FIXTURE_TEST_AAPT" "$report/positive" > "$report/positive.log" 2>&1
for host in WalletDisplayFixtureActivity WalletReviewFixtureActivity; do
    for fault in exported missing-exported missing release; do
        result="$report/$host-$fault"
        if ZCL_FIXTURE_TEST_HOST="org.zclassic.wallet.$host" ZCL_FIXTURE_TEST_FAULT="$fault" \
            bash "$wallet_root/tools/check-android-fixtures.sh" "$report/fault-aapt" \
            "$result" > "$result.log" 2>&1; then
            echo "Fixture isolation regression: accepted $fault host $host" >&2
            exit 1
        fi
        if [[ "$fault" == release ]]; then
            rg -q 'debug host entered release manifest' "$result.log"
        else
            rg -q 'expected exactly one nonexported debug host' "$result.log"
        fi
    done
done
mkdir -p "$report/fault-tools"
cat > "$report/fault-tools/unzip" <<'FIXTURE'
#!/usr/bin/env bash
set -euo pipefail
case "$ZCL_FIXTURE_TEST_FAULT:$2" in
    app-review:*/debug/android-app-debug.apk)
        "$ZCL_FIXTURE_TEST_UNZIP" "$@"
        printf '%s\n' 'assets/review/draft' ;;
    missing-review:*/androidTest/debug/android-app-debug-androidTest.apk)
        "$ZCL_FIXTURE_TEST_UNZIP" "$@" | awk '!/^assets\/review\/previous1$/' ;;
    *) "$ZCL_FIXTURE_TEST_UNZIP" "$@" ;;
esac
FIXTURE
chmod u+x "$report/fault-tools/unzip"
for fault in app-review missing-review; do
    if PATH="$report/fault-tools:$PATH" ZCL_FIXTURE_TEST_FAULT="$fault" \
        bash "$wallet_root/tools/check-android-fixtures.sh" "$ZCL_FIXTURE_TEST_AAPT" \
        "$report/$fault" > "$report/$fault.log" 2>&1; then
        echo "Fixture isolation regression: accepted $fault asset mutation" >&2
        exit 1
    fi
done
rg -q 'public response/review assets entered an application APK' "$report/app-review.log"
rg -q 'expected three public transactions in the test APK' "$report/missing-review.log"
echo 'Fixture isolation regression passed: actual APKs accepted; host and transaction-asset mutations refused.'
