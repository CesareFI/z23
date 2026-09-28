#!/usr/bin/env bash
# Copyright 2026 Rhett Creighton; SPDX-License-Identifier: Apache-2.0.
# Linux x86-64 GCC 13/14 witness: compare direct-source and exact
# preprocessed-stream object bytes for one real TU from the same cwd.
# The observed mismatch requires reuse refusal; this is not an attestation.
set -euo pipefail

if (( $# != 2 )); then
    printf 'usage: %s /absolute/gcc -std=c23|-std=c2x\n' "$0" >&2
    exit 2
fi
compiler=$1
standard=$2
case "$standard" in
    -std=c23|-std=c2x) ;;
    *) printf 'unsupported standard: %s\n' "$standard" >&2; exit 2 ;;
esac
case "$compiler" in
    /*) ;;
    *) printf 'compiler path must be absolute\n' >&2; exit 2 ;;
esac

repo=$(git rev-parse --show-toplevel)
source_tu=platform/modules/base/src/result.c
work=$(mktemp -d /tmp/z23-verify-real-tu.XXXXXX)
trap 'rm -rf -- "$work"' EXIT
flags=(
    "$standard" -g -O3 -march=x86-64-v3 -flto=auto
    -Wall -Wextra -Werror -pedantic
    "-ffile-prefix-map=$repo=/zclassic23"
    -gno-record-gcc-switches -fstack-protector-strong
    -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2 -fcf-protection=full -fPIE
    -Iplatform/modules/sha3/include -Iplatform/modules/base/include
    -D_POSIX_C_SOURCE=200809L
    "-frandom-seed=$source_tu"
)

cd "$repo"
"$compiler" "${flags[@]}" -E "$source_tu" -o "$work/input.i"
"$compiler" "${flags[@]}" -c "$source_tu" -o "$work/result.o"
cp "$work/result.o" "$work/source.o"
"$compiler" "${flags[@]}" -x cpp-output -c "$work/input.i" -o "$work/result.o"

if cmp -s "$work/source.o" "$work/result.o"; then
    equal=1
else
    equal=0
fi
printf 'real_tu=%s compiler=%s standard=%s source_vs_cpp_equal=%d\n' \
    "$source_tu" "$compiler" "$standard" "$equal"
sha256sum "$work/source.o" "$work/result.o"
printf 'compiler_launches=3 reusable_object=%d\n' "$equal"
if (( equal != 0 )); then
    printf 'expected GCC debug/LTO direct-source mismatch disappeared\n' >&2
    exit 1
fi
