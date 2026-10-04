#!/usr/bin/env bash
# Copyright 2026 Rhett Creighton - Apache License 2.0
# Hash the exact names and bytes consumed by a generated view.
set -euo pipefail

if [ "$#" -eq 0 ]; then
    echo 'generated-view-input-key: no input files' >&2
    exit 2
fi

for input in "$@"; do
    if [ ! -f "$input" ]; then
        printf 'generated-view-input-key: missing input: %s\n' "$input" >&2
        exit 2
    fi
done

records="$(sha256sum -- "$@")" || exit 2
printf 'zcl.generated_view_inputs.v1\0%s\0' "$records" | sha256sum | awk '{print $1}'
