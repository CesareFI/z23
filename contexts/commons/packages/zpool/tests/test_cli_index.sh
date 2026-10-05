#!/bin/sh
set -eu
if [ "$#" -ne 1 ] || [ ! -x "$1" ]; then exit 2; fi
cli=$1
failures=0
for op in 'f
' 'o
' 'f'; do
  status=0
  diagnostic=$(printf '%s' "$op" | "$cli" 16 1 2>&1) || status=$?
  case "$diagnostic" in
    'zpool: bad op '* ) matched=yes;;
    * ) matched=no;;
  esac
  if [ "$status" -ne 2 ] || [ "$matched" != yes ]; then
    echo "FAIL missing index: status=$status diagnostic=$diagnostic" >&2
    failures=$((failures + 1))
  fi
done
if [ "$failures" -ne 0 ]; then exit 1; fi
echo 'zpool CLI: missing index refusals passed'
