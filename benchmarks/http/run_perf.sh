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
# Call graphs use dwarf, not fp: drogon and trantor come prebuilt from vcpkg with
# their own flags, so frame pointers are only guaranteed inside our own binary.
# dwarf unwinding costs samples but is the only mode that is fair to both.
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD="${1:-$ROOT/build/release-perf}"
REQUESTS="${2:-20000000}"
PIPELINE="${3:-1}"
WARMUP=$((REQUESTS / 20))
BIN="$BUILD/benchmarks/http"
OUT="${OUT:-$ROOT/benchmarks/http/results}"
mkdir -p "$OUT"

command -v perf >/dev/null || { echo "perf not found"; exit 1; }
paranoid=$(cat /proc/sys/kernel/perf_event_paranoid)
[[ "$paranoid" -le 2 ]] || { echo "perf_event_paranoid=$paranoid too high"; exit 1; }

profile() { # <label> <binary> <port>
    local label="$1" bin="$2" port="$3"
    echo "=== profiling $label (pipeline=$PIPELINE, $REQUESTS requests) ==="
    perf record -q -g --call-graph=dwarf -F 499 -o "$OUT/$label.data" -- \
        taskset -c 0-3 "$BIN/$bin" "$port" 4 >/dev/null 2>&1 &
    local perf_pid=$!
    curl -s --retry 60 --retry-delay 0 --retry-connrefused -o /dev/null "http://127.0.0.1:$port/ping" \
        || { echo "$label never came up"; kill -9 $perf_pid; return 1; }

    taskset -c 4-11 "$BIN/Demiplane.Benchmarks.Http.Bomber" \
        --port "$port" --threads 8 --conns 256 --pipeline "$PIPELINE" \
        --requests "$REQUESTS" --warmup "$WARMUP" --json

    # SIGINT the server; perf flushes when its child exits.
    pkill -INT -f "$bin $port" 2>/dev/null
    wait $perf_pid 2>/dev/null
    for _ in $(seq 1 50); do ss -ltn "sport = :$port" 2>/dev/null | grep -q LISTEN || break; sleep 0.1; done

    echo "--- top 25 symbols, $label ---"
    perf report -i "$OUT/$label.data" --no-children --sort=symbol --stdio 2>/dev/null | head -35
    echo
}

profile demiplane Demiplane.Benchmarks.Http.BenchServer       8080
profile drogon    Demiplane.Benchmarks.Http.DrogonBenchServer 8081

echo "=== syscall counts per request (strace -c on 4 server threads) ==="
echo "profiles written to $OUT/{demiplane,drogon}.data"
echo "inspect with: perf report -i $OUT/demiplane.data --no-children --sort=symbol"
