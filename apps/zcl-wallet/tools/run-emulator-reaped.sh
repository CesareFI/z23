#!/usr/bin/env bash
# Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
set -euo pipefail
if [[ $# -lt 4 || "$3" != -- ]]; then
    echo 'Usage: run-emulator-reaped.sh <sdk-directory> <new-private-report-directory> -- <emulator-arguments...>' >&2
    exit 2
fi
sdk=$(cd -- "$1" && pwd -P)
report=$2
shift 3
wallet_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
if [[ $(uname -s) != Linux || $(uname -m) != x86_64 ]]; then
    echo 'Emulator reaping: only the measured Linux x86-64 SDK is qualified.' >&2
    exit 2
fi
if [[ -n ${LD_PRELOAD:-} || -n ${LD_AUDIT:-} ]]; then
    echo 'Emulator reaping: an existing loader override requires separate qualification.' >&2
    exit 2
fi
# This precise library contains the reviewed post-SIGKILL return site 0x52b40e.
# Do not update the local SDK during a launch. A different SDK must be reviewed
# and measured independently; no guessed symbol offset or unverified fallback.
metrics="$sdk/emulator/lib64/libandroid-emu-metrics.so"
digest=$(sha256sum -- "$metrics")
if [[ ${digest%% *} != c370e2bf32a932690b87f8b0789984f0154cac76f1cc9b10fefa215ec07a8bac ]]; then
    echo 'Emulator reaping: SDK library differs from the qualified bytes.' >&2
    exit 2
fi
umask 077
mkdir -- "$report"
report=$(cd -- "$report" && pwd -P)
if [[ "$report" == *[[:space:]:]* ]]; then
    echo 'Emulator reaping: the loader requires a report path without whitespace or colons.' >&2
    exit 2
fi
clang_bin=${ZCL_HOST_CLANG:-clang-20}
"$clang_bin" -std=c17 -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion \
    -Wshadow -Wformat=2 -Wvla -Wframe-larger-than=4096 -fstack-protector-strong \
    -fPIC -fvisibility=hidden -O2 -shared \
    "$wallet_root/tools/emulator_reap.c" "$wallet_root/tools/emulator_reap_preload.c" \
    -Wl,-z,relro,-z,now,-z,noexecstack -ldl -o "$report/emulator-reap.so"
sha256sum -- "$metrics" "$wallet_root/tools/emulator_reap.c" \
    "$wallet_root/tools/emulator_reap.h" "$wallet_root/tools/emulator_reap_preload.c" \
    "$report/emulator-reap.so" > "$report/inputs.sha256"
printf '%s\n' 'Emulator reaping: qualified SDK timeout wait enabled for this launch.'
exec env LD_PRELOAD="$report/emulator-reap.so" "$sdk/emulator/emulator" -no-window "$@"
