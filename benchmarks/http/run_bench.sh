#!/usr/bin/env bash
#
# HTTP/1.1 throughput: demiplane bench_server vs drogon reference server.
#
#   ./run_bench.sh [build_dir] [requests]
#
# Defaults to build/bench (Release, DMP_ENABLE_LOGGING=OFF) and 100M requests.
#
# Core allocation on this 6-physical-core box (CPUs 0-5 are distinct cores,
# 6-11 are their SMT siblings — cpu6 pairs with cpu0, cpu7 with cpu1, ...):
#
#   PRIMARY  server -c 0-3   client -c 4-11
#            The client's CPUs 6-9 are the SMT siblings of the server's cores,
#            so the two contend inside the same physical cores. This layout
#            cannot measure demiplane's absolute throughput on 4 cores. It can
#            compare two servers under identical contended conditions.
#
#   CONTROL  server -c 0-3   client -c 4,5,10,11
#            Zero core overlap; the client gets 2 physical cores + own siblings.
#            control > primary  => SMT contention was eating server throughput
#            control < primary  => the client was starved at 2 physical cores
#
# Both numbers get reported. Neither is discarded.
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD="${1:-$ROOT/build/bench}"
REQUESTS="${2:-100000000}"
WARMUP=$((REQUESTS / 20))   # 5%
REPS="${REPS:-3}"
BIN="$BUILD/benchmarks/http"

SERVER_CPUS="0-3"
CLIENT_CPUS_PRIMARY="4-11"
CLIENT_CPUS_CONTROL="4,5,10,11"
SERVER_THREADS=4

OUT="${OUT:-$ROOT/benchmark_results/http}"   # gitignored, on real disk
mkdir -p "$OUT"

command -v taskset >/dev/null || { echo "taskset not found"; exit 1; }
for f in Demiplane.Benchmarks.Http.Bomber Demiplane.Benchmarks.Http.BenchServer Demiplane.Benchmarks.Http.DrogonBenchServer; do
    [[ -x "$BIN/$f" ]] || { echo "missing $BIN/$f — build first"; exit 1; }
done

SERVER_PID=""
PORT=""
stop_server() {
    [[ -n "$SERVER_PID" ]] || return 0
    kill -INT "$SERVER_PID" 2>/dev/null
    for _ in $(seq 1 50); do kill -0 "$SERVER_PID" 2>/dev/null || break; sleep 0.1; done
    kill -9 "$SERVER_PID" 2>/dev/null
    wait "$SERVER_PID" 2>/dev/null
    SERVER_PID=""
    # Wait for the listen socket to clear so the next bind() does not race.
    for _ in $(seq 1 50); do ss -ltn "sport = :$PORT" 2>/dev/null | grep -q LISTEN || break; sleep 0.1; done
}
# Backstop: a `timeout` or Ctrl-C kills this shell, not its grandchildren. An
# orphaned bomber saturates every core until it finishes its request budget.
# NB: pkill -x matches the process NAME (comm, truncated to 15 chars), not the
# command line. `pkill -f 'Demiplane.Benchmarks.Http'` also matches any shell whose
# argv contains that string -- including the one running this script, and any
# `cmake --build --target Demiplane.Benchmarks.Http.*` invocation. Do not use -f here.
cleanup() { stop_server; pkill -9 -x 'Demiplane.Bench' 2>/dev/null; }
trap cleanup EXIT INT TERM

# Results are small JSON, but keep the convention: never default onto tmpfs.
fs=$(findmnt -no FSTYPE --target "$(dirname "$OUT")" 2>/dev/null || echo unknown)
case "$fs" in
    tmpfs|ramfs) echo "WARNING: $OUT is on $fs (RAM-backed)" >&2 ;;
esac

# start_server <binary> <port>
start_server() {
    PORT="$2"
    taskset -c "$SERVER_CPUS" "$BIN/$1" "$PORT" "$SERVER_THREADS" >/dev/null 2>&1 &
    SERVER_PID=$!
    curl -s --retry 60 --retry-delay 0 --retry-connrefused -o /dev/null "http://127.0.0.1:$PORT/ping" \
        || { echo "server $1 never came up on $PORT"; exit 1; }
}

# run_one <label> <binary> <port> <pipeline> <client_cpus> <threads> <conns> <rep>
run_one() {
    local label="$1" bin="$2" port="$3" pipe="$4" cpus="$5" thr="$6" conns="$7" rep="$8"
    start_server "$bin" "$port"
    local json
    json=$(taskset -c "$cpus" "$BIN/Demiplane.Benchmarks.Http.Bomber" \
        --host 127.0.0.1 --port "$port" --path /ping \
        --threads "$thr" --conns "$conns" --pipeline "$pipe" \
        --requests "$REQUESTS" --warmup "$WARMUP" --json 2>/dev/null)
    local rc=$?
    stop_server
    if [[ $rc -ne 0 || -z "$json" ]]; then
        echo "{\"label\":\"$label\",\"rep\":$rep,\"error\":\"bomber rc=$rc\"}" | tee -a "$OUT/raw.jsonl"
        return 1
    fi
    echo "{\"label\":\"$label\",\"rep\":$rep,${json#\{}" | tee -a "$OUT/raw.jsonl"
}

echo "requests=$REQUESTS warmup=$WARMUP reps=$REPS build=$BUILD"
echo "writing $OUT/raw.jsonl"
: > "$OUT/raw.jsonl"

for pipe in 1 16; do
    for rep in $(seq 1 "$REPS"); do
        run_one "demiplane/pipe$pipe/primary" Demiplane.Benchmarks.Http.BenchServer       8080 "$pipe" "$CLIENT_CPUS_PRIMARY" 8 256 "$rep"
        run_one "drogon/pipe$pipe/primary"    Demiplane.Benchmarks.Http.DrogonBenchServer 8081 "$pipe" "$CLIENT_CPUS_PRIMARY" 8 256 "$rep"
    done
done

# Control layout: one rep each at pipeline 1, to size the SMT contention.
run_one "demiplane/pipe1/control" Demiplane.Benchmarks.Http.BenchServer       8080 1 "$CLIENT_CPUS_CONTROL" 4 128 1
run_one "drogon/pipe1/control"    Demiplane.Benchmarks.Http.DrogonBenchServer 8081 1 "$CLIENT_CPUS_CONTROL" 4 128 1

echo
echo "=== median rps per label ==="
python3 - "$OUT/raw.jsonl" <<'PY'
import json, statistics, sys, collections
rows = collections.defaultdict(list)
for line in open(sys.argv[1]):
    line = line.strip()
    if not line: continue
    d = json.loads(line)
    if "error" in d:
        print(f"{d['label']:34s} rep{d['rep']}  ERROR {d['error']}")
        continue
    rows[d["label"]].append(d)
print(f"{'label':34s} {'median rps':>12s} {'min':>12s} {'max':>12s} {'p99us':>8s} {'failed':>7s}")
for label, ds in rows.items():
    r = sorted(x["rps"] for x in ds)
    print(f"{label:34s} {statistics.median(r):12,.0f} {r[0]:12,.0f} {r[-1]:12,.0f} "
          f"{ds[0]['p99_us']:8d} {sum(x['failed'] for x in ds):7d}")
PY
