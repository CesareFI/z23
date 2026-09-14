#!/usr/bin/env bash
# Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
# Offline public-fixture extraction; never runs an original node or wallet.
set -euo pipefail
wallet_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
reference=${1:?Usage: project-original-sighashes.sh <original-checkout> <built-seed_sighash_vectors> <new-report-directory>}
oracle=${2:?A locally built oracle executable is required}
report=${3:?A new output directory is required}
commit=14a83d510ffd109d3fa09bf74ebf8c28854a263f
if [[ ! -x "$oracle" ]]; then
    printf '%s\n' 'Build seed_sighash_vectors in the host ZCL_ORACLE profile before extraction.' >&2
    exit 1
fi
git -C "$reference" cat-file -e "$commit:src/test/data/sighash.json"
umask 077
mkdir -- "$report"
cp "$wallet_root/native/tests/zip243-transparent.tsv" "$report/zip243-transparent.tsv"
cp "$wallet_root/native/tests/zip243-reference.LICENSE" "$report/zip243-reference.LICENSE"
git -C "$reference" show "$commit:src/test/data/sighash.json" >"$report/original-sighash.json"
git -C "$reference" show "$commit:src/script/interpreter.cpp" >"$report/original-interpreter.cpp"
git -C "$reference" show "$commit:src/primitives/transaction.h" >"$report/original-transaction.h"
git -C "$reference" show "$commit:src/script/sighashtype.h" >"$report/original-sighashtype.h"
git -C "$reference" show "$commit:src/hash.h" >"$report/original-hash.h"
git -C "$reference" show "$commit:src/test/sighash_tests.cpp" >"$report/original-sighash-tests.cpp"
printf '%s\n' "$commit" >"$report/original-commit.txt"
(
    cd "$report"
    printf '%s\n' '6e10f3e5649c876a8968a3ba13885aeb8dcee8040fd89e7e39651b041d07f30c  original-sighash.json' | sha256sum -c -
    sha256sum -c "$wallet_root/native/tests/zip243-reference.sha256"
)
# These seven TSV fields preserve original row/index/raw type/branch/wire/
# script/result bytes. Upstream's amount is zero. The C oracle verifies all
# 130 selected rows before emitting any projected expected values.
awk -F '"' '
substr($2,1,16)=="0400008085202f89" {
    fields=$5; gsub(/[ \t]/,"",fields); n=split(fields,number,",");
    if(n!=5 || number[1]!="" || number[5]!="") exit 1;
    print NR "\t" number[2] "\t" number[3] "\t" number[4] "\t" $2 "\t" $4 "\t" $6;
    seen++;
}
END { if(seen!=130) exit 1; }
' "$report/original-sighash.json" >"$report/original-v4.tsv"
(
    ulimit -t 30
    "$oracle" "$report/original-v4.tsv" "$report/zip243-transparent.tsv"
) >"$report/sighash-vectors.h" 2>"$report/oracle.log"
sha256sum "$oracle" >"$report/oracle-binary.sha256"
(
    cd "$report"
    sha256sum original-* zip243-* sighash-vectors.h oracle.log oracle-binary.sha256 >SHA256SUMS
)
printf '%s\n' 'Matched 130 original and 2 ZIP 243 hashes; emitted 144 explicitly projected public SIGHASH_ALL fixtures.'
