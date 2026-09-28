#!/usr/bin/env bash
# Copyright 2026 Rhett Creighton; SPDX-License-Identifier: Apache-2.0
#
# Exercises platform/deploy/devbuild (the reference mirror of the installed
# host scheduler, NOT the installed copy) with HOME pointed at a scratch
# directory, so it never touches this host's real locks, queues or
# accounting file. Covers: Z23 lane overlap up to DEVBUILD_Z23_LANES, FIFO
# ordering of --wait waiters, landing priority jumping the queue, QEDC
# staying single-lane, per-job JSON accounting, and ticket cleanup.
set -euo pipefail

root=$(cd "$(dirname "$0")" && pwd)
scratch=$(mktemp -d "${TMPDIR:-/tmp}/devbuild-mirror-test.XXXXXX")
export HOME="$scratch/home"
mkdir -p "$HOME"
state="$HOME/.local/state/development"

pids=()
cleanup() {
    for pid in "${pids[@]}"; do
        [[ -z $pid ]] || wait "$pid" 2>/dev/null || true
    done
    rm -rf -- "$scratch"
}
trap cleanup EXIT

now_ns() { date +%s%N; }

# --- 1. Z23 lane overlap: three concurrent lanes, a fourth waits -----------
# Uses interval overlap (not a fixed wall-clock budget) so the assertion
# holds even when the shared host is heavily loaded and systemd-run/cgroup
# setup is slow: what matters is that the three admitted jobs' [start,end)
# intervals share a common instant, and the fourth only starts once one of
# them has ended, not how many wall-clock milliseconds any of that took.
export DEVBUILD_Z23_LANES=3
"$root/devbuild" --wait --project z23 \
    bash -c 'date +%s%N > "$1"; sleep 5; date +%s%N > "$1.end"' _ "$scratch/lane1" \
    >"$scratch/lane1.log" 2>&1 & pids+=("$!")
"$root/devbuild" --wait --project z23 \
    bash -c 'date +%s%N > "$1"; sleep 5; date +%s%N > "$1.end"' _ "$scratch/lane2" \
    >"$scratch/lane2.log" 2>&1 & pids+=("$!")
"$root/devbuild" --wait --project z23 \
    bash -c 'date +%s%N > "$1"; sleep 5; date +%s%N > "$1.end"' _ "$scratch/lane3" \
    >"$scratch/lane3.log" 2>&1 & pids+=("$!")
sleep 0.5
"$root/devbuild" --wait --project z23 \
    bash -c 'date +%s%N > "$1"; sleep 0.1; date +%s%N > "$1.end"' _ "$scratch/lane4" \
    >"$scratch/lane4.log" 2>&1 & pids+=("$!")
wait "${pids[@]}"
pids=()
s1=$(<"$scratch/lane1"); s2=$(<"$scratch/lane2"); s3=$(<"$scratch/lane3")
s4=$(<"$scratch/lane4")
e1=$(<"$scratch/lane1.end"); e2=$(<"$scratch/lane2.end"); e3=$(<"$scratch/lane3.end")
max_start=$s1; [[ $s2 -gt $max_start ]] && max_start=$s2; [[ $s3 -gt $max_start ]] && max_start=$s3
min_end=$e1; [[ $e2 -lt $min_end ]] && min_end=$e2; [[ $e3 -lt $min_end ]] && min_end=$e3
(( max_start < min_end )) || {
    printf 'three z23 lanes did not overlap (latest start=%d earliest end=%d)\n' \
        "$max_start" "$min_end" >&2
    exit 1
}
(( s4 >= min_end )) || {
    printf 'fourth z23 job started before any lane freed (s4=%d earliest end=%d)\n' \
        "$s4" "$min_end" >&2
    exit 1
}
printf 'devbuild mirror: three z23 lanes overlap, fourth waits for a lane PASS\n'

# --- 2. FIFO ordering of ordinary waiters on the last free lane ------------
"$root/devbuild" --wait --project z23 sleep 4 >"$scratch/hold1.log" 2>&1 & pids+=("$!")
"$root/devbuild" --wait --project z23 sleep 4 >"$scratch/hold2.log" 2>&1 & pids+=("$!")
sleep 0.5
for tag in A B C D E; do
    "$root/devbuild" --wait --project z23 \
        bash -c 'date +%s%N > "$1"; sleep 0.2' _ "$scratch/fifo-$tag" \
        >"$scratch/fifo-$tag.log" 2>&1 & pids+=("$!")
    sleep 0.3
done
wait "${pids[@]}"
pids=()
prev=0
order_ok=1
for tag in A B C D E; do
    ts=$(<"$scratch/fifo-$tag")
    (( ts > prev )) || order_ok=0
    prev=$ts
done
(( order_ok )) || { printf 'FIFO waiters did not start in enqueue order\n' >&2; exit 1; }
printf 'devbuild mirror: FIFO waiter order PASS\n'

# --- 3. Landing priority jumps ordinary --wait waiters ----------------------
"$root/devbuild" --wait --project z23 sleep 4 >"$scratch/prio-hold1.log" 2>&1 & pids+=("$!")
"$root/devbuild" --wait --project z23 sleep 4 >"$scratch/prio-hold2.log" 2>&1 & pids+=("$!")
"$root/devbuild" --wait --project z23 sleep 4 >"$scratch/prio-hold3.log" 2>&1 & pids+=("$!")
sleep 0.5
"$root/devbuild" --wait --project z23 \
    bash -c 'date +%s%N > "$1"; sleep 0.1' _ "$scratch/prio-F" \
    >"$scratch/prio-F.log" 2>&1 & pids+=("$!")
sleep 0.3
"$root/devbuild" --wait --project z23 \
    bash -c 'date +%s%N > "$1"; sleep 0.1' _ "$scratch/prio-G" \
    >"$scratch/prio-G.log" 2>&1 & pids+=("$!")
sleep 0.3
DEVBUILD_PRIORITY=land "$root/devbuild" --wait --project z23 \
    bash -c 'date +%s%N > "$1"; sleep 0.1' _ "$scratch/prio-land" \
    >"$scratch/prio-land.log" 2>&1 & pids+=("$!")
wait "${pids[@]}"
pids=()
land_ts=$(<"$scratch/prio-land")
f_ts=$(<"$scratch/prio-F")
g_ts=$(<"$scratch/prio-G")
(( land_ts < f_ts && land_ts < g_ts )) || {
    printf 'landing priority job did not jump ordinary waiters (land=%d F=%d G=%d)\n' \
        "$land_ts" "$f_ts" "$g_ts" >&2
    exit 1
}
printf 'devbuild mirror: landing priority jumps queue PASS\n'

# --- 4. QEDC stays single-lane ----------------------------------------------
# The 0.5s stagger before launching qedc2 gives qedc1 time to actually queue
# (create its FIFO ticket) first; without it, both processes can race to
# create their tickets close enough together that either may legitimately
# win the (single) lane first, making the "qedc1 must run first" assertion
# meaningless rather than a real single-lane check.
"$root/devbuild" --wait --project qedc \
    bash -c 'date +%s%N > "$1"; sleep 1; date +%s%N > "$1.end"' _ "$scratch/qedc1" \
    >"$scratch/qedc1.log" 2>&1 & pids+=("$!")
sleep 0.5
"$root/devbuild" --wait --project qedc \
    bash -c 'date +%s%N > "$1"; sleep 0.1' _ "$scratch/qedc2" \
    >"$scratch/qedc2.log" 2>&1 & pids+=("$!")
wait "${pids[@]}"
pids=()
qe1=$(<"$scratch/qedc1.end")
qs2=$(<"$scratch/qedc2")
(( qs2 >= qe1 )) || {
    printf 'qedc ran two jobs concurrently (end1=%d start2=%d)\n' "$qe1" "$qs2" >&2
    exit 1
}
printf 'devbuild mirror: qedc single-lane serialization PASS\n'

# --- 5. Per-job JSON accounting, including rc propagation ------------------
jobs_file="$state/devbuild.jobs.jsonl"
[[ -f $jobs_file ]] || { printf 'accounting file missing: %s\n' "$jobs_file" >&2; exit 1; }
rc=0
"$root/devbuild" --wait --project z23 bash -c 'exit 3' >"$scratch/rc.log" 2>&1 || rc=$?
(( rc == 3 )) || { printf 'devbuild did not propagate rc=3 (got %d)\n' "$rc" >&2; exit 1; }
last_line=$(grep '"pid"' "$jobs_file" | tail -1)
[[ -n $last_line ]] || { printf 'no accounting line found\n' >&2; exit 1; }
[[ $last_line == *'"rc":3'* ]] || { printf 'accounting line missing rc:3: %s\n' "$last_line" >&2; exit 1; }
[[ $last_line == *'"project":"z23"'* ]] || { printf 'accounting line missing project z23: %s\n' "$last_line" >&2; exit 1; }
lane_val=$(printf '%s' "$last_line" | sed -n 's/.*"lane":\([0-9]*\).*/\1/p')
[[ $lane_val =~ ^[0-9]+$ ]] && (( lane_val >= 1 )) || {
    printf 'accounting line has non-positive z23 lane: %s\n' "$last_line" >&2; exit 1;
}
wait_s=$(printf '%s' "$last_line" | sed -n 's/.*"wait_s":\([0-9]*\).*/\1/p')
run_s=$(printf '%s' "$last_line" | sed -n 's/.*"run_s":\([0-9]*\).*/\1/p')
[[ $wait_s =~ ^[0-9]+$ ]] || { printf 'wait_s not an integer: %s\n' "$last_line" >&2; exit 1; }
[[ $run_s =~ ^[0-9]+$ ]] || { printf 'run_s not an integer: %s\n' "$last_line" >&2; exit 1; }
line_count=$(grep -c '"pid"' "$jobs_file")
(( line_count >= 12 )) || {
    printf 'expected at least 12 accounting lines, got %d\n' "$line_count" >&2; exit 1;
}
printf 'devbuild mirror: JSON accounting (rc, lane, wait_s, run_s) PASS\n'

# --- 6. No leftover ticket/priority markers ---------------------------------
leftover=0
for f in "$state/z23.queue"/* "$state/z23.priority"/* "$state/qedc.queue"/*; do
    [[ -e $f ]] && leftover=1
done
(( leftover == 0 )) || { printf 'leftover queue/priority marker files remain\n' >&2; exit 1; }
printf 'devbuild mirror: no leftover ticket or priority files PASS\n'
