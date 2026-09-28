#!/usr/bin/env bash
# Copyright 2026 Rhett Creighton; SPDX-License-Identifier: Apache-2.0.
# Ineligible launcher preflight and parser falsification on an unprivileged host.
set -euo pipefail

fail() { printf 'fixed_result_launcher_probe_refuse=%s\n' "$1" >&2; exit 2; }
repo=$(git rev-parse --show-toplevel)
[[ $PWD == "$repo" ]] || fail checkout_cwd
work=$(mktemp -d "$HOME/.z23-launcher-probe.XXXXXX")
trap 'rm -rf -- "$work"' EXIT

/usr/bin/cc -std=c23 -Wall -Wextra -Werror -pedantic \
    -Iplatform/modules/sha3/include -Iplatform/modules/base/include \
    -Iplatform/modules/platform/include \
    tools/verify/fixed_result_launcher.c \
    platform/modules/sha3/src/sha3.c \
    platform/modules/platform/src/os_proc.c -o "$work/launcher"

cat > "$work/check.c" <<'EOF'
#define main launcher_entry
#include "tools/verify/fixed_result_launcher.c"
#undef main

int main(void)
{
    FILE *policy = fopen("tools/verify/fixed_result_launch_policy.v1", "rb");
    char policy_copy[2048];
    if (!policy) return 8;
    size_t policy_len = fread(policy_copy, 1, sizeof(policy_copy), policy);
    if (ferror(policy) || fclose(policy) != 0 ||
        policy_len != strlen(policy_text) ||
        memcmp(policy_copy, policy_text, policy_len) != 0) return 9;
    char input[2048];
    char roots[PIN_COUNT][65];
    size_t at = 0;
    int n = snprintf(input, sizeof(input),
                     "z23verify.fixed_result.pins.v1\n");
    if (n <= 0) return 1;
    at = (size_t)n;
    char digest[65];
    memset(digest, 'a', 64);
    digest[64] = 0;
    for (size_t i = 0; i < PIN_COUNT; i++) {
        n = snprintf(input + at, sizeof(input) - at, "%s=%s\n",
                     pin_names[i], digest);
        if (n <= 0 || (size_t)n >= sizeof(input) - at) return 2;
        at += (size_t)n;
    }
    if (!parse_pins(input, at, roots) || strcmp(roots[10], digest)) return 3;
    input[at - 2] = 'G';
    if (parse_pins(input, at, roots)) return 4;
    input[at - 2] = 'a';
    input[at++] = 'x';
    input[at] = 0;
    if (parse_pins(input, at, roots)) return 5;
    input[at - 1] = 0;
    if (parse_pins(input, at, roots)) return 10;
    at--;
    char *name = strstr(input, "worker_sha3=");
    if (!name) return 6;
    name[0] = 'x';
    if (parse_pins(input, at, roots)) return 7;
    return 0;
}
EOF
/usr/bin/cc -std=c23 -Wall -Wextra -Werror -pedantic \
    -I. -Iplatform/modules/sha3/include -Iplatform/modules/base/include \
    -Iplatform/modules/platform/include \
    "$work/check.c" platform/modules/sha3/src/sha3.c \
    platform/modules/platform/src/os_proc.c -o "$work/check"
"$work/check" || fail parser_false_green

if "$work/launcher" preflight > "$work/out" 2> "$work/err"; then
    fail same_uid_accepted
fi
grep -Fx 'fixed_result_launcher_refuse=root_required' "$work/err" >/dev/null ||
    fail same_uid_wrong_refusal
if "$work/launcher" other > "$work/out" 2> "$work/err"; then
    fail unknown_command_accepted
fi
grep -Fx 'fixed_result_launcher_refuse=request_shape_unsupported' \
    "$work/err" >/dev/null || fail unknown_wrong_refusal
if platform/deploy/fixed-result-launcher-install.sh install \
        > "$work/out" 2> "$work/err"; then
    fail same_uid_install_accepted
fi
grep -Fx 'fixed_result_install_refuse=root_required' "$work/err" >/dev/null ||
    fail same_uid_install_wrong_refusal
printf 'pin_parser=GREEN forged_field=RED trailing_bytes=RED embedded_nul=RED same_uid=REFUSE '
printf 'compiler_invocations=2 executed_launcher=2 installer_refusals=1 '
printf 'proof_launches_avoided=0 attest_eligible=0\n'
