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
    exported:*/debug/android-app-debug.apk)
        "$ZCL_FIXTURE_TEST_AAPT" "$@" | sed 's/:exported(0x01010010)=false/:exported(0x01010010)=true/g' ;;
    release:*/release/android-app-release-unsigned.apk)
        printf '%s\n' 'E: activity' 'A: android:name="org.zclassic.wallet.WalletDisplayFixtureActivity"' ;;
    *) "$ZCL_FIXTURE_TEST_AAPT" "$@" ;;
esac
FIXTURE
chmod u+x "$report/fault-aapt"
bash "$wallet_root/tools/check-android-fixtures.sh" "$ZCL_FIXTURE_TEST_AAPT" "$report/positive" > "$report/positive.log" 2>&1
for fault in exported release; do
    if ZCL_FIXTURE_TEST_FAULT="$fault" bash "$wallet_root/tools/check-android-fixtures.sh" \
        "$report/fault-aapt" "$report/$fault" > "$report/$fault.log" 2>&1; then
        echo "Fixture isolation regression: accepted $fault host" >&2
        exit 1
    fi
done
rg -q 'expected exactly one nonexported debug host' "$report/exported.log"
rg -q 'debug host entered release manifest' "$report/release.log"
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
