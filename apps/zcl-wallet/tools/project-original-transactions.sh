#!/usr/bin/env bash
# Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
# Read-only, offline fixture extraction. Does not run an original node or wallet.
set -euo pipefail
reference=${1:?Usage: project-original-transactions.sh <original-checkout> <new-output-directory>}
report=${2:?A new output directory is required}
commit=14a83d510ffd109d3fa09bf74ebf8c28854a263f
git -C "$reference" cat-file -e "$commit:src/test/data/sighash.json"
mkdir -- "$report"
git -C "$reference" show "$commit:src/test/data/sighash.json" > "$report/original-sighash.json"
git -C "$reference" show "$commit:src/primitives/transaction.h" > "$report/original-transaction.h"
printf '%s\n' "$commit" > "$report/original-commit.txt"
# Each selected upstream vector has short canonical transparent fields and zero
# expiry. Preserve its exact header, inputs, outputs and lock/expiry bytes. The
# original random generator always adds shielded fields, so replace ONLY that
# tail with valueBalance=0 and three empty vectors. These are projections, not
# untouched upstream transactions or evidence of original-node acceptance.
awk -F '"' -v out="$report" '
function byte(n, a,b) {
    a=index("0123456789abcdef",substr(s,2*n+1,1))-1;
    b=index("0123456789abcdef",substr(s,2*n+2,1))-1;
    if(a<0 || b<0) exit 1;
    return 16*a+b;
}
function count(max, n) { n=byte(p++); if(n>max) exit 1; return n; }
NR==203 || NR==208 || NR==296 {
    s=$2; if(substr(s,1,16)!="0400008085202f89") exit 1;
    p=8; ins=count(8); if(ins==0)exit 1;
    for(i=0;i<ins;i++){p+=36; n=count(128);p+=n+4;}
    outs=count(16); if(outs==0)exit 1;
    for(i=0;i<outs;i++){p+=8;n=count(25);p+=n;}
    p+=4; if(substr(s,2*p+1,8)!="00000000")exit 1;p+=4;
    if(p*2>=length(s))exit 1;
    print s > (out "/original-" NR ".hex");
    print substr(s,1,p*2) "0000000000000000000000" > (out "/projected-" NR ".hex");
    print NR,ins,outs,p > (out "/prefixes.txt");
    seen++;
}
END { if(seen!=3)exit 1; }
' "$report/original-sighash.json"
for row in 203 208 296; do
    xxd -r -p "$report/projected-$row.hex" > "$report/projected-$row.bin"
    openssl dgst -sha256 -binary "$report/projected-$row.bin" |
        openssl dgst -sha256 -binary |
        od -An -v -tx1 |
        awk '{for(i=1;i<=NF;i++)a[++n]=$i} END{if(n!=32)exit 1;for(i=n;i>0;i--)printf "%s",a[i];print ""}' \
        > "$report/projected-$row.txid"
done
(cd "$report"; sha256sum original-* projected-* prefixes.txt > SHA256SUMS)
echo 'Projected three pinned-original transparent prefixes; independent SHA256d IDs recorded.'
