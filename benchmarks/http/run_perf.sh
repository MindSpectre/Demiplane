#!/usr/bin/env bash
#
# Where does the per-request time go? Profiles both servers under identical load.
#
#   ./run_perf.sh [build_dir] [requests] [pipeline]
#
# Uses build/release-perf, which sets DMP_KEEP_FRAME_POINTERS=ON. That matters:
# CMakeLists.txt appends -fomit-frame-pointer in Release as a *target* compile
# option, which lands after CMAKE_CXX_FLAGS on the command line and would beat a
# preset that merely added -fno-omit-frame-pointer to the flags.
#
# CALL GRAPHS USE fp, NOT dwarf. dwarf snapshots ~8 KB of stack per sample; at
# 499 Hz across four busy threads that is >1 GB per profile, and `perf script`
# could not unwind it here anyway (only kernel frames survived). fp needs frame
# pointers: our binary has them via DMP_KEEP_FRAME_POINTERS, and Fedora ships
# glibc with them. Drogon/trantor come prebuilt from vcpkg without, so their user
# stacks truncate — acceptable, since the question is where OUR time goes.
#
# OUTPUT MUST NOT LAND ON tmpfs. /tmp on this box is a 16 GB tmpfs and swap is
# zram (compresses into RAM, no disk backing). Filling tmpfs there does not
# OOM-kill; it livelocks kswapd at 100% CPU and needs a hard reboot. Learned the
# hard way. The guard below refuses to run rather than repeat it.
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD="${1:-$ROOT/build/release-perf}"
REQUESTS="${2:-20000000}"
PIPELINE="${3:-1}"
WARMUP=$((REQUESTS / 20))
BIN="$BUILD/benchmarks/http"
# benchmark_results/ is gitignored and lives on /home (real disk).
OUT="${OUT:-$ROOT/benchmark_results/http}"
mkdir -p "$OUT"

command -v perf >/dev/null || { echo "perf not found"; exit 1; }
paranoid=$(cat /proc/sys/kernel/perf_event_paranoid)
[[ "$paranoid" -le 2 ]] || { echo "perf_event_paranoid=$paranoid too high"; exit 1; }

# --- refuse RAM-backed output ------------------------------------------------
fs=$(findmnt -no FSTYPE --target "$OUT" 2>/dev/null || echo unknown)
case "$fs" in
    tmpfs|ramfs)
        echo "REFUSING: $OUT is on $fs (RAM-backed). perf data would be written to memory."
        echo "Set OUT= to a path on real disk."
        exit 1 ;;
esac
avail_kb=$(df -Pk "$OUT" | awk 'NR==2 {print $4}')
[[ "$avail_kb" -ge 5242880 ]] || { echo "REFUSING: <5 GiB free on $OUT"; exit 1; }

SERVER_PID=""; PERF_PID=""; BOMBER_PID=""
# NB: pkill -x matches the process NAME (comm, truncated to 15 chars), not the
# command line. `pkill -f 'Demiplane.Benchmarks.Http'` also matches any shell whose
# argv contains that string -- including the one running this script, and any
# `cmake --build --target Demiplane.Benchmarks.Http.*` invocation. Do not use -f here.
cleanup() {
    for p in "$BOMBER_PID" "$SERVER_PID" "$PERF_PID"; do
        [[ -n "$p" ]] && kill -INT "$p" 2>/dev/null
    done
    sleep 1
    for p in "$BOMBER_PID" "$SERVER_PID" "$PERF_PID"; do
        [[ -n "$p" ]] && kill -9 "$p" 2>/dev/null
    done
    # Backstop: nothing of ours may outlive this script, even if it was killed.
    pkill -9 -x 'Demiplane.Bench' 2>/dev/null
    wait 2>/dev/null
}
trap cleanup EXIT INT TERM

profile() { # <label> <binary> <port>
    local label="$1" bin="$2" port="$3"
    echo "=== profiling $label (pipeline=$PIPELINE, $REQUESTS requests) ==="
    perf record -q -g --call-graph=fp -F 999 -o "$OUT/$label.data" -- \
        taskset -c 0-3 "$BIN/$bin" "$port" 4 >/dev/null 2>&1 &
    PERF_PID=$!
    curl -s --retry 60 --retry-delay 0 --retry-connrefused -o /dev/null "http://127.0.0.1:$port/ping" \
        || { echo "$label never came up"; return 1; }

    taskset -c 4-11 "$BIN/Demiplane.Benchmarks.Http.Bomber" \
        --port "$port" --threads 8 --conns 256 --pipeline "$PIPELINE" \
        --requests "$REQUESTS" --warmup "$WARMUP" --json &
    BOMBER_PID=$!
    wait $BOMBER_PID; BOMBER_PID=""

    pkill -INT -f "$bin $port" 2>/dev/null
    wait $PERF_PID 2>/dev/null; PERF_PID=""
    for _ in $(seq 1 50); do ss -ltn "sport = :$port" 2>/dev/null | grep -q LISTEN || break; sleep 0.1; done

    echo "--- $label.data: $(du -h "$OUT/$label.data" | cut -f1) ---"
    perf report -i "$OUT/$label.data" --no-children -g none --sort=symbol --stdio -q 2>/dev/null | head -25 | cut -c1-120
    echo
}

profile demiplane Demiplane.Benchmarks.Http.BenchServer       8080
profile drogon    Demiplane.Benchmarks.Http.DrogonBenchServer 8081

echo "profiles written to $OUT/{demiplane,drogon}.data"
echo "inspect with: perf report -i $OUT/demiplane.data --no-children -g none --sort=symbol"
