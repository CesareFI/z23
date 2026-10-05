#!/bin/sh
set -eu
if [ "$#" -ne 1 ] || [ ! -x "$1" ]; then exit 2; fi
cli=$1
failures=0
for block_size in 1 17 63; do
  status=0
  output=$(printf 's\n' | "$cli" "$block_size" 3) || status=$?
  if [ "$status" -ne 0 ] || [ "$output" != 's -> free 3/3' ]; then
    echo "FAIL requested capacity: block_size=$block_size status=$status output=$output" >&2
    failures=$((failures + 1))
  fi
done
if [ "$failures" -ne 0 ]; then exit 1; fi
echo 'zpool CLI: requested block counts passed'
