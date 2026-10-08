#!/usr/bin/env bash
# Copyright 2026 Rhett Creighton - Licensed under Apache-2.0
# Gate wrapper — see tools/lint/lintc/gate_network_tool_hardening.c for the
# full purpose comment.
exec "$(dirname "$0")/../../build/bin/z23-lint" check-network-tool-hardening "$@"
