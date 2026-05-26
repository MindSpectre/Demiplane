#!/usr/bin/env bash
# Run the flagship benchmark across 10 steady RPS levels and 10 burst sizes,
# capturing JSON output for each config. Effective RPS is the same for both
# arrival patterns (burst effective_rps = size / cooldown_s = size * 2).
#
# Usage: scripts/run_flagship_rps_sweep.sh [--skip-build]
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/../../../.." && pwd)"
BIN="$REPO_ROOT/build/release/benchmarks/multithread/resource_pool/Demiplane.Benchmarks.Multithread.ResourcePool.FlagshipBurst"
OUT_DIR="/tmp/flagship_rps_sweep"

skip_build=0
for arg in "$@"; do
    [[ "$arg" == "--skip-build" ]] && skip_build=1
done

if [[ "$skip_build" -eq 0 ]]; then
    echo "=== Building FlagshipBurst ==="
    cmake --build "$REPO_ROOT/build/release" \
          --target Demiplane.Benchmarks.Multithread.ResourcePool.FlagshipBurst
fi

mkdir -p "$OUT_DIR/steady" "$OUT_DIR/burst"

# Steady: 10 RPS levels. total = max(rps*5, 1000) to keep each mode ~5s.
STEADY_RPS=(100 250 500 750 1000 2000 3000 5000 7500 10000)

echo ""
echo "=== Steady sweep (${#STEADY_RPS[@]} configs) ==="
for rps in "${STEADY_RPS[@]}"; do
    total=$(( rps * 5 ))
    (( total < 1000 )) && total=1000
    out="$OUT_DIR/steady/rps_${rps}.json"
    echo "  rps=$rps  total=$total  -> $out"
    "$BIN" --config=all --dispatch=all --load=steady \
           --rps="$rps" --total="$total" \
           --out="$out"
done

# Burst: 10 burst sizes. Cooldown is fixed at 500ms (struct default).
# effective_rps = size / 0.5 = size * 2 — matches the steady RPS values above.
# --bursts=10 keeps each run ~5s (10 * 500ms = 5s cooldown).
BURST_SIZES=(50 125 250 375 500 1000 1500 2500 3750 5000)

echo ""
echo "=== Burst sweep (${#BURST_SIZES[@]} configs) ==="
for size in "${BURST_SIZES[@]}"; do
    eff_rps=$(( size * 2 ))
    out="$OUT_DIR/burst/size_${size}.json"
    echo "  burst_size=$size  effective_rps=$eff_rps  -> $out"
    "$BIN" --config=all --dispatch=all --load=burst \
           --burst-size="$size" --bursts=10 \
           --out="$out"
done

echo ""
echo "=== Done. Results in $OUT_DIR ==="
echo "Run:  python3 $REPO_ROOT/benchmarks/multithread/resource_pool/scripts/plot_flagship_rps_sweep.py"
