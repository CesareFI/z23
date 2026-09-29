#!/usr/bin/env bash
# Copyright 2026 Rhett Creighton - Apache License 2.0
#
# Lint gate — sqlite cursor lifetime: a stepped sqlite3_stmt must be
# released (sqlite3_finalize / sqlite3_reset) BEFORE any returning error
# macro (LOG_FAIL, LOG_ERR, LOG_NULL, LOG_RETURN, GUARD, GUARD_NOT_NULL,
# GUARD_NOT_NULL_RET_NULL, GUARD_NOT_NULL_ERR) fires in the same function.
#
# Those macros RETURN from the caller (platform/modules/util/include/util/
# log_macros.h), so a finalize written after them is dead code. The parked
# SELECT keeps the connection's WAL read snapshot pinned and every later
# writer on that connection fails with SQLITE_BUSY_SNAPSHOT — the
# 2026-09-29 node.db write-plane wedge class (fixes 278f328c56, ad6920b8d4).
# The safe idiom is finalize-then-log-and-return; this gate keeps the tree
# there. No baseline: the tree was verified clean when the gate was added.
exec "$(dirname "$0")/../../build/bin/z23-lint" check-sqlite-cursor-lifetime "$@"
