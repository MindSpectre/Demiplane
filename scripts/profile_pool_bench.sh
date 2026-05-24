#!/usr/bin/env bash
# Collect hotspot profiles for ResourcePool benchmarks:
#   - perf record + flamegraph (sampled CPU hot paths)
#   - llvm-profdata (instrumented, exact function counts)
#
# Runs against a fixed (subject, worker-count, scenarios) slice — by default
# AcqFor100us at 64 workers across three scenarios — to keep total runtime
# bounded. Adjust the SCENARIOS / WORKERS / SUBJECT lists below if needed.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$REPO_ROOT"

PREFIX="Demiplane.Benchmarks.Multithread.ResourcePool"
SUBJECT_TARGET="AcqFor10us"
SUBJECT_BENCH="RP_AcqFor_10us"
SCENARIOS=("TimeoutPressure" "AsioPostSteady" "Steady")
WORKERS=64
PROF="benchmark_results/profiles"
mkdir -p "$PROF"

# ─── perf record (release build, no rebuild needed) ────────────────────────
echo "=== Building release target ==="
cmake --preset=release
cmake --build build/release --target "${PREFIX}.${SUBJECT_TARGET}"

PERF_BIN="build/release/benchmarks/multithread/resource_pool/${PREFIX}.${SUBJECT_TARGET}"
for sc in "${SCENARIOS[@]}"; do
    echo "=== perf record: $sc ==="
    taskset -c 0-9 perf record -F 999 -g \
        -o "$PROF/${sc}.data" -- \
        "$PERF_BIN" \
        --benchmark_filter="BM_${SUBJECT_BENCH}_${sc}/${WORKERS}" \
        --pin=0 \
        --benchmark_min_time=2s

    if command -v stackcollapse-perf.pl >/dev/null && command -v flamegraph.pl >/dev/null; then
        perf script -i "$PROF/${sc}.data" \
            | stackcollapse-perf.pl \
            | flamegraph.pl > "$PROF/${sc}.svg"
        echo "  wrote $PROF/${sc}.svg"
    else
        echo "  (stackcollapse-perf.pl / flamegraph.pl not on PATH — skipping SVG)"
    fi
done

# ─── llvm-profdata (instrumented build) ────────────────────────────────────
echo
echo "=== Building release-instrprof target ==="
cmake --preset=release-instrprof
cmake --build build/release-instrprof --target "${PREFIX}.${SUBJECT_TARGET}"

INSTR_BIN="build/release-instrprof/benchmarks/multithread/resource_pool/${PREFIX}.${SUBJECT_TARGET}"
for sc in "${SCENARIOS[@]}"; do
    echo "=== llvm-profdata: $sc ==="
    LLVM_PROFILE_FILE="$PROF/${sc}-%p.profraw" \
        taskset -c 0-9 "$INSTR_BIN" \
        --benchmark_filter="BM_${SUBJECT_BENCH}_${sc}/${WORKERS}" \
        --pin=0 \
        --benchmark_min_time=2s
done

llvm-profdata merge -sparse "$PROF"/*.profraw -o "$PROF/merged.profdata"
llvm-profdata show -all-functions -topn=40 "$PROF/merged.profdata" \
    > "$PROF/topn.txt"
echo
echo "Top-40 function counts written to $PROF/topn.txt"
echo "Flamegraphs (if generated) at $PROF/*.svg"
