#!/usr/bin/env bash
# Copyright 2026 Rhett Creighton - Apache License 2.0
# Synthetic observations only: no node, clocks, or proof authority required.
set -euo pipefail
script_dir="$(cd "$(dirname "$0")" && pwd)"
report="${ZCL_PERF_REPORT_UNDER_TEST:-$script_dir/zcode_async_proof_perf_report.sh}"
scratch="$(mktemp -d "${TMPDIR:-/tmp}/zcl-perf-report-test.XXXXXX")"
trap 'rm -rf "$scratch"' EXIT
mkdir -p "$scratch/node"
check() {
    grep -Eq "$1" "$scratch/report" || {
        printf 'proof-perf selftest: missing %s\n' "$1" >&2
        cat "$scratch/report" >&2
        exit 1
    }
}
worker() {
    printf '[zcode.proof_perf] action=%064d stage=worker_execute at_unix_us=1000005 %s child_cpu_us=0 sandbox_prepare_us=1 execution_us=2 output_cas_us=3 receipt_sign_us=4 lookup_us=5 input_reconstruction_us=6 output_verify_us=7 revalidation_us=8 projection_us=9 input_bytes=10 output_bytes=11 host_processes=1 host_executor_launches=1 child_reported_processes_unverified=2 child_reported_compiler_processes_unverified=1 child_reported_test_processes_unverified=0 cache_hit=0\n' "$1" "$2" >>"$scratch/node/node.log"
}
for i in 1 2 3; do
    printf '{"foreground_request_creation_us":1,"durable_action_lookup_dedup_us":2,"local_submit_us":3,"local_first_feedback_us":4,"live_rpc_admission_us":5,"live_rpc_request_bytes":6,"live_rpc_response_bytes":7}\n' >"$scratch/async-submit-$i-result.json"
    for stage in foreground_return requester_dispatch remote_admission worker_lease worker_result_publish requester_result acceptance_ready; do
        printf '[zcode.proof_perf] action=%064d stage=%s at_unix_us=1000000 retry=0 context_prepare_us=1 peer_selection_us=2 request_submit_us=3 context_bytes=4 request_wire_bytes=5 context_cache_hit=0 admission_us=6 transferred_bytes=7 claim_us=8 queue_us=9 receipt_verification_us=10 projection_us=11 result_wire_bytes=12 local_verification_us=13\n' "$i" "$stage" >>"$scratch/node/node.log"
    done
done
worker 1 total_us=9999999
worker 2 total_us=10000000
worker 3 total_us=10000001
bash "$report" "$scratch" >"$scratch/report"
check '^remote_execution_us n=3 p50_us=10000000 p95_us=10000001 mean_us=10000000 max_us=10000001'
check '^remote_cpu_us n=3 p50_us=0 p95_us=0 mean_us=0 max_us=0 expected=3 missing=0 invalid=0 complete=true$'
check '^worker_host_processes n=3 .* expected=3 missing=0 invalid=0 complete=true$'
check '^child_reported_compiler_processes_unverified n=3 .* expected=3 missing=0 invalid=0 complete=true$'
check '^worker_cache_hits=0/3$'

# A second signed reproduction action shares the original work submission.
# Its missing foreground is allowed only when the result names that exact ID.
reproduction_action="$(printf '%064d' 3)"
cp "$scratch/node/node.log" "$scratch/complete-node.log"
cp "$scratch/async-submit-3-result.json" "$scratch/complete-result.json"
grep -vF "action=$reproduction_action stage=foreground_return" \
    "$scratch/complete-node.log" >"$scratch/node/node.log"
sed "s/}$/,\"reproduction_action_id\":\"$reproduction_action\"}/" \
    "$scratch/complete-result.json" >"$scratch/async-submit-3-result.json"
bash "$report" "$scratch" >"$scratch/report"
check '^reproduction_without_foreground=1 unobserved_reproduction_actions=0 invalid_reproduction_records=0$'
check "^reproduction_without_foreground_action=$reproduction_action$"
check '^background_total_precise_us n=2 .* expected=2 missing=0 invalid=0 complete=true$'
cp "$scratch/complete-result.json" "$scratch/async-submit-3-result.json"
if bash "$report" "$scratch" >"$scratch/report"; then
    printf 'proof-perf selftest: unmarked missing foreground did not refuse\n' >&2
    exit 1
fi
check '^background_total_precise_us n=2 .* expected=3 missing=1 invalid=0 complete=false$'
sed 's/}$/,"reproduction_action_id":"invalid"}/' \
    "$scratch/complete-result.json" >"$scratch/async-submit-3-result.json"
if bash "$report" "$scratch" >"$scratch/report"; then
    printf 'proof-perf selftest: invalid reproduction identity did not refuse\n' >&2
    exit 1
fi
check '^reproduction_without_foreground=0 unobserved_reproduction_actions=0 invalid_reproduction_records=1$'
cp "$scratch/complete-result.json" "$scratch/async-submit-3-result.json"
cp "$scratch/complete-node.log" "$scratch/node/node.log"

# A structurally valid result with one requested metric absent is incomplete,
# while every other field remains measured and valid.
printf '{"durable_action_lookup_dedup_us":2,"local_submit_us":3,"local_first_feedback_us":4,"live_rpc_admission_us":5,"live_rpc_request_bytes":6,"live_rpc_response_bytes":7}\n' >"$scratch/async-submit-3-result.json"
if bash "$report" "$scratch" >"$scratch/report"; then
    printf 'proof-perf selftest: missing foreground metric did not refuse\n' >&2
    exit 1
fi
check '^foreground_request_creation_us n=2 .* expected=3 missing=1 invalid=0 complete=false$'

# Restore the complete baseline before exercising malformed and oversized
# worker observations below.
printf '{"foreground_request_creation_us":1,"durable_action_lookup_dedup_us":2,"local_submit_us":3,"local_first_feedback_us":4,"live_rpc_admission_us":5,"live_rpc_request_bytes":6,"live_rpc_response_bytes":7}\n' >"$scratch/async-submit-3-result.json"

# Repeated executions are samples too. Missing, invalid, and large spans must
# neither disappear nor be confused with real measured zero durations.
worker 1 ''
worker 1 total_us=0
worker 1 total_us=4294967296
worker 1 total_us=-1
worker 1 total_us=bad
worker 1 total_us=9007199254740992
printf '{}\n' >"$scratch/async-submit-4-result.json"
if bash "$report" "$scratch" >"$scratch/report"; then
    printf 'proof-perf selftest: invalid samples did not refuse\n' >&2
    exit 1
fi
check '^remote_execution_us n=5 p50_us=10000000 p95_us=4294967296 mean_us=864993459 max_us=4294967296 expected=9 missing=1 invalid=3 complete=false$'
check '^foreground_request_creation_us n=3 .* expected=4 missing=1 invalid=0 complete=false$'
check '^worker_cache_hits=0/9$'

# An entirely unmeasured metric remains explicitly absent, and incomplete
# lifecycles are accounted for in cross-host deltas.
printf '[zcode.proof_perf] action=%064d stage=worker_execute at_unix_us=1000001 total_us=10000000\n' 4 >"$scratch/node/node.log"
printf '[zcode.proof_perf] action=invalid stage=worker_execute at_unix_us=1 total_us=1\n' >>"$scratch/node/node.log"
if bash "$report" "$scratch" >"$scratch/report"; then
    printf 'proof-perf selftest: absent metrics did not refuse\n' >&2
    exit 1
fi
check '^remote_execution_us n=1 .* max_us=10000000 expected=1 missing=0 invalid=0 complete=true$'
check '^remote_cpu_us n=0 .* expected=1 missing=1 invalid=0 complete=false$'
check '^background_total_precise_us n=0 .* expected=1 missing=1 invalid=0 complete=false$'
check '^lifecycle_records=2 invalid_lifecycle_records=1$'
check '^context_cache_unmeasured=0 worker_cache_unmeasured=1$'
printf 'proof-perf-report-selftest: PASS (boundary, missing, zero, retries, invalid, wide values, complete accounting)\n'
