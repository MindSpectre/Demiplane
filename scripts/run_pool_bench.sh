#!/usr/bin/env bash
# Build and run all 6 ResourcePool benchmark binaries, twice each
# (floating workers + pinned 1:1), confined to cores 0..9 via taskset.
# Output: /tmp/pool_bench_results/{floating,pinned}/<subject>.json
#
# Usage:
#   scripts/run_pool_bench.sh                # build + run everything
#   scripts/run_pool_bench.sh --skip-build   # skip cmake build step

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$REPO_ROOT"

SKIP_BUILD=0
for arg in "$@"; do
    case "$arg" in
        --skip-build) SKIP_BUILD=1 ;;
        *) echo "unknown arg: $arg" >&2; exit 2 ;;
    esac
done

PREFIX="Demiplane.Benchmarks.Multithread.ResourcePool"
TARGETS=(
    "${PREFIX}.Try"
    "${PREFIX}.AcqFor1us"
    "${PREFIX}.AcqFor2us"
    "${PREFIX}.AcqFor10us"
    "${PREFIX}.Pinned"
    "${PREFIX}.MutexBase"
    "${PREFIX}.ArpAcqFor1us"
    "${PREFIX}.ArpAcqFor2us"
    "${PREFIX}.ArpAcqFor10us"
)

if [[ "$SKIP_BUILD" -eq 0 ]]; then
    cmake --preset=release
    cmake --build build/release --target "${TARGETS[@]}"
fi

OUT=/tmp/pool_bench_results
mkdir -p "$OUT/floating" "$OUT/pinned"

# subject-slug : cmake-target-name
declare -a SUBJECTS=(
    "try:Try"
    "acqfor_1us:AcqFor1us"
    "acqfor_2us:AcqFor2us"
    "acqfor_10us:AcqFor10us"
    "rp_pinned:Pinned"
    "mutex_base:MutexBase"
    "arp_acqfor_1us:ArpAcqFor1us"
    "arp_acqfor_2us:ArpAcqFor2us"
    "arp_acqfor_10us:ArpAcqFor10us"
)

BIN_DIR="build/release/benchmarks/multithread/resource_pool"

for pair in "${SUBJECTS[@]}"; do
    slug="${pair%%:*}"
    target="${pair##*:}"
    bin="${BIN_DIR}/${PREFIX}.${target}"
    if [[ ! -x "$bin" ]]; then
        echo "missing binary: $bin" >&2
        exit 1
    fi
    echo "=== $slug (floating) ==="
    taskset -c 0-9 "$bin" --pin=0 \
        --benchmark_out="$OUT/floating/${slug}.json" \
        --benchmark_out_format=json
    echo "=== $slug (pinned)   ==="
    taskset -c 0-9 "$bin" --pin=1 \
        --benchmark_out="$OUT/pinned/${slug}.json" \
        --benchmark_out_format=json
done

echo
echo "Done. JSON files written under $OUT/{floating,pinned}/"
