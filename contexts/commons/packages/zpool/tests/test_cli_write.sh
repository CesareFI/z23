#!/bin/sh
set -eu
if [ "$#" -ne 1 ] || [ ! -x "$1" ] || [ ! -c /dev/full ]; then exit 2; fi
cli=$1
failures=0
for op in s a; do
  status=0
  output=$(printf '%s\n' "$op" | "$cli" 16 1) || status=$?
  case "$op" in
    s) expected='s -> free 1/1';;
    a) expected='a -> 0';;
  esac
  if [ "$status" -ne 0 ] || [ "$output" != "$expected" ]; then
    echo "FAIL output control: op=$op status=$status output=$output" >&2
    failures=$((failures + 1))
  fi
  status=0
  diagnostic=$(printf '%s\n' "$op" | "$cli" 16 1 2>&1 >/dev/full) || status=$?
  if [ "$status" -ne 2 ] || [ "$diagnostic" != 'zpool: write error' ]; then
    echo "FAIL write refusal: op=$op status=$status diagnostic=$diagnostic" >&2
    failures=$((failures + 1))
  fi
done
if [ "$failures" -ne 0 ]; then exit 1; fi
echo 'zpool CLI: write-error refusals passed'
