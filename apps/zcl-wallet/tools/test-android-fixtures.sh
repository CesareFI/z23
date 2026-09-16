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
    process-*:* )
        if [[ "$5" != */"${ZCL_FIXTURE_TEST_VARIANT:?}"/* ]]; then
            exec "$ZCL_FIXTURE_TEST_AAPT" "$@"
        fi
        "$ZCL_FIXTURE_TEST_AAPT" "$@" | awk -v fault="${ZCL_FIXTURE_TEST_FAULT#process-}" '
            /E:/ {
                decoder=0; wallet=0; scanner=0
                if (/E: application / && fault == "application") {
                    print; print "        A: android:process(0x01010011)=\":other\""; next
                }
                if (/E: manifest / && fault == "shared-user") {
                    print; print "    A: android:sharedUserId(0x0101000b)=\"public.fixture\""; next
                }
            }
            /android:name/ {
                decoder=index($0, "\"org.zclassic.wallet.ScanDecodeService\"")
                wallet=index($0, "\"org.zclassic.wallet.MainActivity\"")
                scanner=index($0, "\"org.zclassic.wallet.CameraScanActivity\"")
                if (decoder && fault == "missing-decoder") next
                if (decoder && fault == "reordered") print "            A: android:isolatedProcess(0x010103a9)=true"
                if (wallet && fault == "wallet") {
                    print; print "            A: android:process(0x01010011)=\":other\""; next
                }
            }
            decoder && /android:isolatedProcess/ {
                if (fault == "missing-isolated" || fault == "reordered") next
                if (fault == "shared-decoder") sub(/=true$/, "=false")
            }
            decoder && /android:exported/ {
                if (fault == "missing-exported") next
                if (fault == "exported-decoder") sub(/=false$/, "=true")
            }
            decoder && /android:process\(/ {
                if (fault == "missing-process") next
                if (fault == "other-decoder") gsub(/:qrdecode/, ":other")
            }
            decoder && /android:stopWithTask/ {
                if (fault == "missing-stop") next
                if (fault == "retained-decoder") sub(/=true$/, "=false")
            }
            scanner && /android:exported/ && fault == "exported-scanner" { sub(/=false$/, "=true") }
            { print }
            END {
                if (fault == "other-isolated") {
                    print "          E: service"
                    print "            A: android:name(0x01010003)=\"public.OtherDecoder\""
                    print "            A: android:isolatedProcess(0x010103a9)=true"
                }
                if (fault == "multiprocess") {
                    print "          E: provider"
                    print "            A: android:name(0x01010003)=\"public.OtherProvider\""
                    print "            A: android:multiprocess(0x01010013)=true"
                }
                if (fault == "duplicate-decoder") {
                    print "          E: service"
                    print "            A: android:name(0x01010003)=\"org.zclassic.wallet.ScanDecodeService\""
                    print "            A: android:exported(0x01010010)=false"
                    print "            A: android:process(0x01010011)=\":qrdecode\""
                    print "            A: android:isolatedProcess(0x010103a9)=true"
                    print "            A: android:stopWithTask(0x0101036a)=true"
                }
            }' ;;
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
for variant in debug release; do
    for fault in missing-decoder missing-isolated shared-decoder missing-exported exported-decoder \
        missing-process other-decoder missing-stop retained-decoder wallet application shared-user \
        exported-scanner other-isolated duplicate-decoder multiprocess; do
        result="$report/process-$variant-$fault"
        if ZCL_FIXTURE_TEST_VARIANT="$variant" ZCL_FIXTURE_TEST_FAULT="process-$fault" \
            bash "$wallet_root/tools/check-android-fixtures.sh" "$report/fault-aapt" \
            "$result" > "$result.log" 2>&1; then
            echo "Fixture isolation regression: accepted $variant process mutation $fault" >&2
            exit 1
        fi
        rg -q "$variant APK violates the wallet/decoder process boundary" "$result.log"
    done
    # Attribute order carries no authority. Equivalent merged metadata passes.
    ZCL_FIXTURE_TEST_VARIANT="$variant" ZCL_FIXTURE_TEST_FAULT=process-reordered \
        bash "$wallet_root/tools/check-android-fixtures.sh" "$report/fault-aapt" \
        "$report/reordered-$variant" > "$report/reordered-$variant.log" 2>&1
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
echo 'Fixture isolation regression passed: actual APKs accepted; process, host and asset mutations refused.'
