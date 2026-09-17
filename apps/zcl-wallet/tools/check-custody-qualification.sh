#!/usr/bin/env bash
# Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
set -euo pipefail
aapt=${1:?Usage: check-custody-qualification.sh <aapt2> <application-apk> <test-apk> <report>}
application=${2:?Application APK is required}
test_apk=${3:?Instrumentation APK is required}
report=${4:?Report directory is required}
mkdir -p "$report"
for kind in application test; do
    apk=$application
    suffix=
    if [[ "$kind" == test ]]; then apk=$test_apk; suffix=.test; fi
    "$aapt" dump xmltree --file AndroidManifest.xml "$apk" > "$report/$kind-manifest.txt"
    if ! awk -v kind="$kind" -v package="org.zclassic.wallet.dev.qualification$suffix" '
        function quoted(line) {
            sub(/^[^=]*="/, "", line); sub(/".*$/, "", line); return line
        }
        /E: / {
            node=$0; sub(/^.*E: /, "", node); sub(/ .*/, "", node)
            if (node == "application") applications++
            if (node == "instrumentation") instruments++
        }
        /A: package=/ { packages++; if (node != "manifest" || quoted($0) != package) bad=1 }
        /android:sharedUserId\(/ { bad=1 }
        /android:testOnly\(/ {
            tests++; if (node != "application" || $0 !~ /=true$/) bad=1
        }
        /android:debuggable\(/ {
            debugs++; if (node != "application" || $0 !~ /=true$/) bad=1
        }
        /android:allowBackup\(/ {
            backups++; if (node != "application" || $0 !~ /=false$/) bad=1
        }
        /android:targetPackage\(/ {
            targets++; if (node != "instrumentation" || quoted($0) != "org.zclassic.wallet.dev.qualification") bad=1
        }
        /android:name\(/ && node == "instrumentation" {
            runners++; if (quoted($0) != "androidx.test.runner.AndroidJUnitRunner") bad=1
        }
        END {
            if (kind == "application" && (instruments || targets || backups != 1)) bad=1
            if (kind == "test" && (instruments != 1 || targets != 1 || runners != 1)) bad=1
            exit (bad || packages != 1 || applications != 1 || tests != 1 || debugs != 1)
        }
    ' "$report/$kind-manifest.txt"; then
        echo "Custody qualification: $kind APK violates test-only identity isolation" >&2
        exit 1
    fi
done
sha256sum "$application" "$test_apk" > "$report/apks.sha256"
echo 'Custody qualification identity passed; this is not hardware custody evidence.'
