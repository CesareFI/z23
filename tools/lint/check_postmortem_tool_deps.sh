#!/usr/bin/env bash
# Copyright 2026 Rhett Creighton - Apache License 2.0
# The standalone link must follow its headers and vendored zlib archive.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"
target=build/bin/postmortem_to_scenario

if ! make --no-print-directory -q "$target"; then
    echo 'check_postmortem_tool_deps: clean target is unexpectedly stale' >&2
    exit 1
fi

for input in \
    engine/modules/sim/include/sim/postmortem.h \
    platform/modules/base/include/base/format_attribute.h \
    vendor/lib/libz.a; do
    if make --no-print-directory -q -W "$input" "$target"; then
        echo "check_postmortem_tool_deps: $input did not trigger rebuild" >&2
        exit 1
    else
        status=$?
        if [ "$status" -ne 1 ]; then
            echo "check_postmortem_tool_deps: make failed for $input (status $status)" >&2
            exit 1
        fi
    fi
done

echo 'check_postmortem_tool_deps: PASS'
