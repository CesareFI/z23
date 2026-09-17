#!/usr/bin/env bash
# Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
set -euo pipefail
wallet_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
export ZCL_QUALIFY_AAPT=${1:?Usage: test-custody-qualification.sh <aapt2> <application-apk> <test-apk> <report>}
application=${2:?Application APK is required}
test_apk=${3:?Instrumentation APK is required}
report=${4:?Report directory is required}
mkdir -p "$report"
cat > "$report/fault-aapt" <<'FIXTURE'
#!/usr/bin/env bash
set -euo pipefail
if [[ "$5" != "$ZCL_QUALIFY_TARGET" ]]; then exec "$ZCL_QUALIFY_AAPT" "$@"; fi
"$ZCL_QUALIFY_AAPT" "$@" | awk -v fault="$ZCL_QUALIFY_FAULT" '
    /E:/ { instrument=($0 ~ /E: instrumentation /) }
    /android:name/ && instrument {
        if (fault == "missing-runner") next
        if (fault == "runner") gsub(/androidx.test.runner.AndroidJUnitRunner/, "public.WrongRunner")
    }
    /E: manifest / && fault == "shared-user" {
        print; print "    A: android:sharedUserId(0x0101000b)=\"public.fixture\""; next
    }
    /A: package=/ {
        if (fault == "missing-package") next
        if (fault == "package") gsub(/org.zclassic.wallet.dev.qualification/, "public.wrong")
    }
    /android:testOnly/ {
        if (fault == "missing-test-only") next
        if (fault == "test-only") sub(/=true$/, "=false")
    }
    /android:debuggable/ {
        if (fault == "missing-debuggable") next
        if (fault == "debuggable") sub(/=true$/, "=false")
    }
    /android:allowBackup/ {
        if (fault == "missing-backup") next
        if (fault == "backup") sub(/=false$/, "=true")
    }
    /android:targetPackage/ {
        if (fault == "missing-target") next
        if (fault == "target") gsub(/org.zclassic.wallet.dev.qualification/, "public.wrong")
    }
    {print}'
FIXTURE
chmod u+x "$report/fault-aapt"
validator="$wallet_root/tools/check-custody-qualification.sh"
bash "$validator" "$ZCL_QUALIFY_AAPT" "$application" "$test_apk" "$report/positive"
for kind in application test; do
    export ZCL_QUALIFY_TARGET=$application
    faults=(package missing-package test-only missing-test-only debuggable missing-debuggable shared-user)
    if [[ "$kind" == test ]]; then
        ZCL_QUALIFY_TARGET=$test_apk
        faults+=(target missing-target runner missing-runner)
    else faults+=(backup missing-backup)
    fi
    for fault in "${faults[@]}"; do
        result="$report/$kind-$fault"
        if ZCL_QUALIFY_FAULT="$fault" bash "$validator" "$report/fault-aapt" \
            "$application" "$test_apk" "$result" > "$result.log" 2>&1; then
            echo "Custody qualification regression: accepted $kind $fault" >&2
            exit 1
        fi
        rg -q "$kind APK violates test-only identity isolation" "$result.log"
    done
done
echo 'Custody qualification identity: unchanged control and 20 negative metadata cases passed.'
