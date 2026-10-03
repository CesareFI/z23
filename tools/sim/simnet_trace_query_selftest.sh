#!/usr/bin/env bash
# Copyright 2026 Rhett Creighton - Apache License 2.0
# Exercise physical NDJSON boundaries against the standalone query binary.
set -euo pipefail
export LC_ALL=C

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BIN="${1:-$ROOT/build/bin/simnet_trace_query}"
WORK="$(mktemp -d "${TMPDIR:-/tmp}/simnet-trace-query.XXXXXX")"
trap 'rm -rf -- "$WORK"' EXIT

fail()
{
    printf 'simnet_trace_query_selftest: FAIL: %s\n' "$*" >&2
    exit 1
}

[ "$#" -le 1 ] || fail 'expected at most one binary path'
[ -x "$BIN" ] || fail 'query binary is not executable'

query()
{
    "$BIN" "--file=$1" "--node=$2" > "$WORK/out" 2> "$WORK/err" ||
        fail 'query failed'
}

summary()
{
    grep -Fqx "simnet_trace_query: $1 lines matched" "$WORK/err" ||
        fail "wrong match count (wanted $1)"
}

# The tail of the first physical line is valid JSON. It must not become a
# second record when the line exceeds the fixed input buffer.
awk 'BEGIN {
    for (i = 0; i < 65535; i++) printf " ";
    print "{\"node_id\":7,\"event\":\"false-tail\",\"seq\":1}";
    print "{\"node_id\":8,\"event\":\"real\",\"seq\":2}";
}' > "$WORK/oversize"
query "$WORK/oversize" 7
[ ! -s "$WORK/out" ] || fail 'oversize tail was accepted as a record'
summary '0/2'
grep -Fqx 'simnet_trace_query: skipping malformed line 1' "$WORK/err" ||
    fail 'oversize line was not diagnosed'
query "$WORK/oversize" 8
printf '%s\n' '{"node_id":8,"event":"real","seq":2}' > "$WORK/expected"
cmp -s "$WORK/expected" "$WORK/out" || fail 'valid following line was lost'
summary '1/2'

# Exactly 65535 bytes before LF fit; one extra byte must be rejected whole.
awk 'BEGIN {
    prefix="{\"node_id\":9,\"event\":\"boundary\",\"pad\":\"";
    suffix="\"}";
    printf "%s", prefix;
    for (i = length(prefix) + length(suffix); i < 65535; i++) printf "x";
    printf "%s\n", suffix;
}' > "$WORK/boundary"
query "$WORK/boundary" 9
cmp -s "$WORK/boundary" "$WORK/out" || fail 'maximal valid line changed'
summary '1/1'
awk 'BEGIN {
    prefix="{\"node_id\":9,\"event\":\"boundary\",\"pad\":\"";
    suffix="\"}";
    printf "%s", prefix;
    for (i = length(prefix) + length(suffix); i < 65536; i++) printf "x";
    printf "%s\n", suffix;
}' > "$WORK/over-boundary"
query "$WORK/over-boundary" 9
[ ! -s "$WORK/out" ] || fail 'over-boundary record was accepted'
summary '0/1'

# A NUL in one physical record must not let a later fragment impersonate a
# record. CRLF remains an ordinary accepted line ending.
printf '{"node_id":7,"event":"nul"\000,"seq":1}\n' > "$WORK/binary"
printf '{"node_id":10,"event":"crlf","seq":3}\r\n' >> "$WORK/binary"
query "$WORK/binary" 7
[ ! -s "$WORK/out" ] || fail 'NUL-bearing record was accepted'
summary '0/2'
query "$WORK/binary" 10
printf '%s\n' '{"node_id":10,"event":"crlf","seq":3}' > "$WORK/expected"
cmp -s "$WORK/expected" "$WORK/out" || fail 'CRLF control changed'
summary '1/2'

printf '%s' '{"node_id":11,"event":"eof","seq":4}' > "$WORK/unterminated"
query "$WORK/unterminated" 11
printf '%s\n' '{"node_id":11,"event":"eof","seq":4}' > "$WORK/expected"
cmp -s "$WORK/expected" "$WORK/out" || fail 'final line without LF was lost'
summary '1/1'

: > "$WORK/empty"
query "$WORK/empty" 7
[ ! -s "$WORK/out" ] || fail 'empty file produced a record'
summary '0/0'
if "$BIN" "--file=$WORK" > "$WORK/out" 2> "$WORK/err"; then
    fail 'directory input was accepted as a trace'
fi
grep -Eq 'simnet_trace_query: (cannot open|read failed):? ' "$WORK/err" ||
    fail 'directory input failed without a read/open diagnostic'

printf '%s\n' 'simnet_trace_query_selftest: PASS'
