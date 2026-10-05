#!/bin/sh
# Linux rejects reading a directory through stdin.
set -eu
if [ "$#" -ne 1 ] || [ ! -x "$1" ]; then exit 2; fi
cli=$1
"$cli" 16 1 </dev/null
status=0
diagnostic=$("$cli" 16 1 <. 2>&1) || status=$?
if [ "$status" -ne 2 ] || [ "$diagnostic" != 'zpool: read error' ]; then
  echo "FAIL read refusal: status=$status diagnostic=$diagnostic" >&2
  exit 1
fi
echo 'zpool CLI: read-error refusal passed'
