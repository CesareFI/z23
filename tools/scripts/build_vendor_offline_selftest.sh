#!/usr/bin/env bash
# Copyright 2026 Rhett Creighton - Apache License 2.0
# Prove offline vendor mode refuses a cache miss before invoking a downloader.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
bash "$ROOT/tools/scripts/vendor_lock_selftest.sh"
SANDBOX="$(mktemp -d "${TMPDIR:-/tmp}/zcl-vendor-offline-selftest.XXXXXX")"
trap 'rm -rf "$SANDBOX"' EXIT HUP INT TERM

fail()
{
    printf 'build_vendor_offline_selftest: FAIL: %s\n' "$*" >&2
    exit 1
}

mkdir -p "$SANDBOX/tools/scripts" "$SANDBOX/tools/dev" "$SANDBOX/vendor/.cache" \
    "$SANDBOX/vendor/lib" "$SANDBOX/vendor/include" "$SANDBOX/bin"
cp "$ROOT/tools/scripts/build_vendor.sh" \
    "$ROOT/tools/scripts/vendor_provenance_lib.sh" \
    "$ROOT/tools/scripts/source_identity_lib.sh" "$SANDBOX/tools/scripts/"
for dependency in platform/modules/platform/src/clock.c \
    platform/modules/platform/include/platform/clock.h \
    platform/modules/util/include/util/log_macros.h \
    platform/modules/base/include/base/log_macros.h \
    platform/modules/base/include/base/format_attribute.h \
    platform/modules/base/include/base/log_level.h; do
    mkdir -p "$SANDBOX/$(dirname "$dependency")"
    cp "$ROOT/$dependency" "$SANDBOX/$dependency"
done
cp "$ROOT/tools/vendor_lock.c" "$SANDBOX/tools/"
cp "$ROOT/tools/dev/build-epoch-key.sh" "$SANDBOX/tools/dev/"

for tool in curl wget; do
    printf '%s\n' '#!/usr/bin/env bash' \
        'printf "%s\\n" "$0 $*" >>"$DOWNLOADER_CONTACT_LOG"' \
        'exit 97' > "$SANDBOX/bin/$tool"
    chmod +x "$SANDBOX/bin/$tool"
done

contact_log="$SANDBOX/downloader-contact.log"
: > "$contact_log"
if output="$(cd "$SANDBOX" && \
        DOWNLOADER_CONTACT_LOG="$contact_log" \
        PATH="$SANDBOX/bin:$PATH" ZCL_VENDOR_OFFLINE=1 \
        tools/scripts/build_vendor.sh libz.a 2>&1)"; then
    fail 'missing archive unexpectedly built in offline mode'
fi
[[ "$output" == *'offline cache miss or checksum failure: zlib-1.3.1.tar.gz'* ]] ||
    fail 'cache-miss refusal did not name the missing pinned archive'
[ ! -s "$contact_log" ] || fail 'offline mode invoked a downloader'

# The compiler driver grounds slash-containing relative tools at the source
# root. Compilation must use that same executable from a different cwd.
mkdir -p "$SANDBOX/elsewhere/tools"
# Freeze the qualified driver before the relative fixture overrides HOSTCC.
# Match the production token grammar; never evaluate a compiler command.
fixture_host_cc="${HOSTCC:-cc}"
[[ "$fixture_host_cc" =~ ^[A-Za-z0-9_./:+,=%-]+([[:space:]]+[A-Za-z0-9_./:+,=%-]+)*$ ]] ||
    fail 'fixture HOSTCC contains unsupported shell syntax'
read -r -a fixture_host_argv <<<"$fixture_host_cc"
case "${fixture_host_argv[0]}" in
    /*) ;;
    */*) fixture_host_argv[0]="$ROOT/${fixture_host_argv[0]}" ;;
    *) fixture_host_argv[0]="$(command -v -- "${fixture_host_argv[0]}")" ||
           fail 'fixture host compiler unavailable' ;;
esac
[[ "${fixture_host_argv[0]}" == /* && -f "${fixture_host_argv[0]}" &&
   -x "${fixture_host_argv[0]}" ]] || fail 'fixture host compiler unavailable'
{
    printf '%s\n' '#!/usr/bin/env bash'
    printf 'exec'
    printf ' %q' "${fixture_host_argv[@]}"
    printf ' "$@"\n'
} > "$SANDBOX/tools/native-cc"
printf '%s\n' '#!/usr/bin/env bash' 'exit 97' > "$SANDBOX/elsewhere/tools/native-cc"
chmod +x "$SANDBOX/tools/native-cc" "$SANDBOX/elsewhere/tools/native-cc"
if output="$(cd "$SANDBOX/elsewhere" && HOSTCC=tools/native-cc \
    ZCL_VENDOR_OFFLINE=1 "$SANDBOX/tools/scripts/build_vendor.sh" libz.a 2>&1)"; then
    fail 'root-scoped compiler fixture unexpectedly built missing archive'
fi
[[ "$output" == *'offline cache miss or checksum failure: zlib-1.3.1.tar.gz'* ]] ||
    fail 'bootstrap compiled with a different executable than its compiler identity'

# A C driver with no C++ front end installed is an ordinary host shape (cc is
# one GCC major, g++ another). The lock helper is C only, so its identity
# must still be observable there instead of refusing every vendor build.
if command -v c++ >/dev/null 2>&1; then
    {
        printf '%s\n' '#!/usr/bin/env bash'
        printf '%s\n' 'previous=""'
        printf '%s\n' 'for argument in "$@"; do'
        printf '%s\n' '    if [[ "$previous" == -x && "$argument" == c++ ]]; then'
        printf '%s\n' "        printf '%s\\n' \"cc: fatal error: cannot execute 'cc1plus': posix_spawnp: No such file or directory\" >&2"
        printf '%s\n' '        exit 1'
        printf '%s\n' '    fi'
        printf '%s\n' '    previous="$argument"'
        printf '%s\n' 'done'
        printf 'exec'
        printf ' %q' "${fixture_host_argv[@]}"
        printf ' "$@"\n'
    } > "$SANDBOX/tools/c-only-cc"
    chmod +x "$SANDBOX/tools/c-only-cc"
    if output="$(cd "$SANDBOX" && HOSTCC=tools/c-only-cc \
        ZCL_VENDOR_OFFLINE=1 tools/scripts/build_vendor.sh libz.a 2>&1)"; then
        fail 'C-only compiler fixture unexpectedly built missing archive'
    fi
    [[ "$output" == *'offline cache miss or checksum failure: zlib-1.3.1.tar.gz'* ]] ||
        fail 'a C driver without a C++ front end could not identify the lock helper compiler'
fi

# Bad bootstrap identities must refuse before touching an existing helper.
cp -R "$SANDBOX/build/bin/vendor-lock" "$SANDBOX/helper-cache-before"
cp "$SANDBOX/tools/dev/build-epoch-key.sh" "$SANDBOX/key-driver-before"
printf '%s\n' '#!/usr/bin/env bash' 'exit 0' > "$SANDBOX/tools/dev/build-epoch-key.sh"
if output="$(ZCL_VENDOR_OFFLINE=1 "$SANDBOX/tools/scripts/build_vendor.sh" libz.a 2>&1)"; then
    fail 'empty compiler identity admitted'
fi
[[ "$output" == *'vendor lock compiler identity invalid'* ]] || fail 'compiler identity refusal unnamed'
cp "$SANDBOX/key-driver-before" "$SANDBOX/tools/dev/build-epoch-key.sh"
cp "$SANDBOX/tools/scripts/vendor_provenance_lib.sh" "$SANDBOX/provenance-before"
for hash_behavior in 'return 1' 'return 0'; do
    cp "$SANDBOX/provenance-before" "$SANDBOX/tools/scripts/vendor_provenance_lib.sh"
    printf '\nvp_sha256_file() { %s; }\n' "$hash_behavior" >> "$SANDBOX/tools/scripts/vendor_provenance_lib.sh"
    if output="$(ZCL_VENDOR_OFFLINE=1 "$SANDBOX/tools/scripts/build_vendor.sh" libz.a 2>&1)"; then
        fail 'failed or empty source hash admitted'
    fi
    [[ "$output" == *'vendor lock source hash'* ]] || fail 'source hash refusal unnamed'
done
cp "$SANDBOX/provenance-before" "$SANDBOX/tools/scripts/vendor_provenance_lib.sh"
diff -r "$SANDBOX/helper-cache-before" "$SANDBOX/build/bin/vendor-lock" >/dev/null ||
    fail 'identity refusal modified cached helper bytes'
mkdir "$SANDBOX/unknown-legacy.lock"
if output="$(VENDOR_LOCK_HELD=1 VENDOR_LOCK_DIR="$SANDBOX/unknown-legacy.lock" \
    ZCL_VENDOR_OFFLINE=1 "$SANDBOX/tools/scripts/build_vendor.sh" libz.a 2>&1)"; then
    fail 'forged environment bypassed unknown legacy lock'
fi
[[ "$output" == *vendor_lock_legacy_directory_refused* ]] || fail 'legacy refusal unnamed'
[[ -d "$SANDBOX/unknown-legacy.lock" ]] || fail 'unknown legacy owner removed'

if ZCL_VENDOR_OFFLINE=invalid "$ROOT/tools/scripts/build_vendor.sh" \
        --check-provenance >/dev/null 2>&1; then
    fail 'invalid offline policy value was accepted'
fi

# Exercise the production Make include boundaries with missing vendor inputs.
# A fresh directory per invocation has no marker or archives. Replace only
# the vendor builder with a contact marker: no compiler, network, or checkout
# build state is needed to observe whether Make attempts vendor bootstrap.
probe_mk="$SANDBOX/bootstrap.mk"
{
    printf '%s\n' 'VENDOR_LIBS := vendor/lib/missing.a'
    awk '/^ZCL_TOR_PROVENANCE_GOALS :=/ { copying=1 }
         copying { print }
         /^ZCL_HOTSWAP_LOOP_ONLY :=/ { copying=0 }' "$ROOT/Makefile"
    awk '/^ZCL_PORTABLE_FRONTDOOR_GOALS :=/ { copying=1 }
         /^# Refuse a compiler/ { copying=0 }
         copying { print }' "$ROOT/Makefile"
    awk '/^VENDOR_BOOTSTRAP_MK :=/ { copying=1 }
         /^# Generated view headers/ { copying=0 }
         copying { print }' "$ROOT/Makefile"
    printf '%s\n' 'ZCL_TOR := full' 'TOR_MISSING_ARCHIVES := vendor/tor/libtor.a'
    awk '/^ZCL_TOR_SKIP_GOALS :=/ { copying=1 }
         /^# The stub is reachable/ { copying=0 }
         copying { print }' "$ROOT/Makefile"
    cat <<'MAKE'
.DEFAULT_GOAL := z23
.PHONY: z23 windows-headless-run windows-headless-run-selftest
z23 print-node-c23-srcs help doctor doctor-build doctor-env timings agent-dev-status \
print-CFLAGS print-DEV-CFLAGS print-LDFLAGS print-DEV-LDFLAGS print-build-flags \
windows-headless-run windows-headless-run-selftest build/bin/z23-headless-run.exe $(ZCL_TOR_PROVENANCE_GOALS):
	@printf 'lean=%s\n' '$(ZCL_HOTSWAP_LOOP_ONLY)'
$(VENDOR_BOOTSTRAP_MK):
	@mkdir -p build/identity
	@printf '%s\n' contacted > vendor-contact
	@printf '%s\n' '# vendor boundary established' > $@
$(TOR_BOOTSTRAP_MK):
	@mkdir -p build/identity
	@printf '%s\n' contacted > tor-contact
	@printf '%s\n' '# Tor boundary established' > $@
MAKE
} > "$probe_mk"

probe_number=0
probe_bootstrap()
{
    local expected="$1"
    shift
    probe_number=$((probe_number + 1))
    local fixture="$SANDBOX/bootstrap-$probe_number"
    mkdir -p "$fixture"
    if ! make --no-print-directory -C "$fixture" -f "$probe_mk" "$@" \
            > "$fixture/output" 2>&1; then
        cat "$fixture/output" >&2
        fail "bootstrap selection probe failed: $*"
    fi
    if [ "$expected" = skip ] || [ "$expected" = query ]; then
        if [ "$expected" = skip ]; then
            grep -q '^lean=1$' "$fixture/output" ||
                fail "bootstrap helper entered authoritative node parse: $*"
        elif grep -q '^lean=1$' "$fixture/output"; then
            fail "source query skipped the full node source declarations: $*"
        fi
        [ ! -e "$fixture/vendor-contact" ] ||
            fail "nonlink goals invoked vendor bootstrap: $*"
        [ ! -e "$fixture/tor-contact" ] ||
            fail "nonlink goals invoked Tor bootstrap: $*"
    else
        if grep -q '^lean=1$' "$fixture/output"; then
            fail "node goals entered lean helper parse: $*"
        fi
        [ -s "$fixture/vendor-contact" ] ||
            fail "node goals skipped vendor bootstrap: $*"
        [ -s "$fixture/tor-contact" ] ||
            fail "node goals skipped Tor bootstrap: $*"
    fi
}

for goal in windows-headless-run windows-headless-run-selftest \
        build/bin/z23-headless-run.exe build/bin/z23-tor-provenance \
        tools/tor-provenance z23-tor-provenance; do
    probe_bootstrap skip "$goal"
    probe_bootstrap require "$goal" z23
done
for goal in print-node-c23-srcs \
    print-CFLAGS print-DEV-CFLAGS print-LDFLAGS print-DEV-LDFLAGS print-build-flags; do
    probe_bootstrap query "$goal"
    probe_bootstrap require "$goal" z23
done
# These reports need no node parse; mixed builds retain authoritative bootstrap.
probe_bootstrap skip help
probe_bootstrap require help z23
probe_bootstrap skip timings
probe_bootstrap require timings z23
probe_bootstrap skip agent-dev-status
probe_bootstrap require agent-dev-status z23
probe_bootstrap skip doctor-build
probe_bootstrap require doctor-build z23
probe_bootstrap skip doctor
probe_bootstrap require doctor z23
probe_bootstrap skip doctor-env
probe_bootstrap require doctor-env z23
probe_bootstrap skip windows-headless-run windows-headless-run-selftest
probe_bootstrap require z23
probe_bootstrap require

# The real status recipe is read-only even without a compiler. Isolate every
# dev-lane path so this fixture cannot inspect an operator's active service.
status_env=(
    ZCL_AGENT_DEV_UNIT=z23-query-fixture-nonexistent.service
    ZCL_AGENT_DEV_DATADIR="$SANDBOX/no-datadir"
    ZCL_AGENT_DEV_BIN="$SANDBOX/no-bin"
    ZCL_AGENT_SRC_BIN="$SANDBOX/no-src"
    ZCL_AGENT_JSONQ="$SANDBOX/no-jsonq"
    ZCL_DEV_GENERATION_ROOT="$SANDBOX/no-generations"
    ZCL_DEV_WATCH_STATE_DIR="$SANDBOX/no-watch"
    ZCL_QUALITY_STATE_DIR="$SANDBOX/no-quality"
    ZCL_AGENT_INDEX_STATUS_PATH="$SANDBOX/no-index"
    ZCL_DEV_BENCH_OUTPUT="$SANDBOX/no-bench"
    ZCL_BIN_DIR="$SANDBOX/no-ccache"
    ZCL_BOOTSTRAP_CC=/nonexistent
    ZCL_VENDOR_OFFLINE=1
)
if ! env "${status_env[@]}" make -s --no-print-directory -C "$ROOT" \
        ZCL_USE_CCACHE=1 CC=/nonexistent \
        ZCL_VENDOR_LIB="$SANDBOX/no-vendor" agent-dev-status ARGS=--json \
        > "$SANDBOX/status-make.json" 2> "$SANDBOX/status-make.err"; then
    cat "$SANDBOX/status-make.err" >&2
    fail 'exact agent-dev-status required a compiler'
fi
env "${status_env[@]}" "$ROOT/tools/dev/agent-dev-status.sh" --json \
    > "$SANDBOX/status-direct.json"
cmp -s "$SANDBOX/status-make.json" "$SANDBOX/status-direct.json" ||
    fail 'exact agent-dev-status changed the report'
[ ! -e "$SANDBOX/no-ccache" ] && [ ! -e "$SANDBOX/no-vendor" ] ||
    fail 'exact agent-dev-status built cache or vendor inputs'
if env "${status_env[@]}" make -s --no-print-directory -C "$ROOT" \
        ZCL_USE_CCACHE=0 CC=/nonexistent agent-dev-status z23 ARGS=--json \
        > "$SANDBOX/status-mixed.out" 2> "$SANDBOX/status-mixed.err" ||
   ! grep -Fq 'C23 toolchain check failed' "$SANDBOX/status-mixed.err"; then
    fail 'mixed agent-dev-status build skipped compiler preflight'
fi

# The accelerator doctor must still explain a missing compiler. Its exact
# report should match the direct script; a mixed node build must not skip preflight.
if ! env ZCL_BIN_DIR="$SANDBOX/doctor-cache" ZCL_BOOTSTRAP_CC=/nonexistent \
        ZCL_VENDOR_OFFLINE=1 make -s --no-print-directory -C "$ROOT" \
        ZCL_USE_CCACHE=1 CC=/nonexistent ZCL_VENDOR_LIB="$SANDBOX/doctor-vendor" \
        doctor-build > "$SANDBOX/doctor-make.out" 2> "$SANDBOX/doctor-make.err"; then
    cat "$SANDBOX/doctor-make.err" >&2
    fail 'exact doctor-build required a compiler'
fi
"$ROOT/tools/dev/doctor-build.sh" > "$SANDBOX/doctor-direct.out"
cmp -s "$SANDBOX/doctor-make.out" "$SANDBOX/doctor-direct.out" ||
    fail 'exact doctor-build changed the report'
[ ! -e "$SANDBOX/doctor-cache" ] && [ ! -e "$SANDBOX/doctor-vendor" ] ||
    fail 'exact doctor-build built cache or vendor inputs'
if env ZCL_VENDOR_OFFLINE=1 make -s --no-print-directory -C "$ROOT" \
        ZCL_USE_CCACHE=0 CC=/nonexistent doctor-build z23 \
        > "$SANDBOX/doctor-mixed.out" 2> "$SANDBOX/doctor-mixed.err" ||
   ! grep -Fq 'C23 toolchain check failed' "$SANDBOX/doctor-mixed.err"; then
    fail 'mixed doctor-build skipped compiler preflight'
fi

# The prerequisite doctor must diagnose a host before the configured build
# compiler works, while a mixed node build still requires that compiler.
if ! env ZCL_BIN_DIR="$SANDBOX/prereq-cache" ZCL_BOOTSTRAP_CC=/nonexistent \
        ZCL_VENDOR_OFFLINE=1 make -s --no-print-directory -C "$ROOT" \
        ZCL_USE_CCACHE=1 CC=/nonexistent ZCL_VENDOR_LIB="$SANDBOX/prereq-vendor" \
        doctor > "$SANDBOX/prereq-make.out" 2> "$SANDBOX/prereq-make.err"; then
    cat "$SANDBOX/prereq-make.err" >&2
    fail 'exact doctor required the configured build compiler'
fi
"$ROOT/tools/scripts/doctor.sh" > "$SANDBOX/prereq-direct.out"
cmp -s "$SANDBOX/prereq-make.out" "$SANDBOX/prereq-direct.out" ||
    fail 'exact doctor changed the prerequisite report'
[ ! -e "$SANDBOX/prereq-cache" ] && [ ! -e "$SANDBOX/prereq-vendor" ] ||
    fail 'exact doctor built cache or vendor inputs'
if env ZCL_VENDOR_OFFLINE=1 make -s --no-print-directory -C "$ROOT" \
        ZCL_USE_CCACHE=0 CC=/nonexistent doctor z23 \
        > "$SANDBOX/prereq-mixed.out" 2> "$SANDBOX/prereq-mixed.err" ||
   ! grep -Fq 'C23 toolchain check failed' "$SANDBOX/prereq-mixed.err"; then
    fail 'mixed doctor skipped compiler preflight'
fi

if env ZCL_VENDOR_OFFLINE=1 make -s --no-print-directory -C "$ROOT" \
        ZCL_USE_CCACHE=0 CC=/nonexistent doctor-env z23 \
        > "$SANDBOX/doctor-env-mixed.out" 2> "$SANDBOX/doctor-env-mixed.err" ||
   ! grep -Fq 'C23 toolchain check failed' "$SANDBOX/doctor-env-mixed.err"; then
    fail 'mixed doctor-env skipped compiler preflight'
fi

printf '%s\n' \
    'build_vendor_offline_selftest: PASS downloader_contacted=false cache_miss_refused=true launcher_vendor_and_tor_skipped=true source_query_vendor_and_tor_skipped=true mixed_goals_bootstrap=true'
