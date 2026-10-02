#!/usr/bin/env bash
# Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
# The headers must be rebuilt after any C input linked into their generator.
set -euo pipefail

make -pRrn templates 2>/dev/null | awk '
BEGIN {
    headers["contexts/wallet/views/include/views/wallet_templates_gen.h:"] = 1
    headers["contexts/explorer/views/include/views/site_css.h:"] = 1
    headers["contexts/commons/views/include/views/install_script_gen.h:"] = 1
}
$1 == "build/bin/gen_templates:" {
    line = $0
    sub(/^[^:]+:[[:space:]]*/, "", line)
    sub(/[[:space:]]*\|.*/, "", line)
    count = split(line, words, /[[:space:]]+/)
    for (i = 1; i <= count; i++)
        if (words[i] ~ /\.c$/) {
            sources[words[i]] = 1
            source_count++
        }
    tools++
}
$1 in headers {
    line = $0
    sub(/[[:space:]]*\|.*/, "", line)
    normal[$1] = " " line " "
    seen++
}
END {
    if (tools != 1 || seen != 3 || source_count == 0) {
        print "template generator dependency check: missing Make rules" > "/dev/stderr"
        exit 1
    }
    for (source in sources)
        for (header in headers)
            if (index(normal[header], " " source " ") == 0) {
                print "template generator dependency check: " header " misses " source > "/dev/stderr"
                bad = 1
            }
    if (bad)
        exit 1
    print "template generator dependency check: PASS"
}'
