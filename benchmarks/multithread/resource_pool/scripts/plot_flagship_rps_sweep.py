#!/usr/bin/env python3
"""
Plot flagship RPS-sweep results: steady and burst, each as a figure with three
latency panels (p50 / p95 / p99).  Four lines per panel:
  sync@10  |  async-pull  |  async-ded  |  async-chan

Input:  /tmp/flagship_rps_sweep/{steady,burst}/*.json
Output: benchmark_results/flagship_rps_sweep/steady_latency_vs_rps.png
        benchmark_results/flagship_rps_sweep/burst_latency_vs_rps.png
"""

from __future__ import annotations

import json
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.ticker as mticker

SWEEP_DIR = Path("/tmp/flagship_rps_sweep")
REPO_ROOT = Path(__file__).resolve().parents[4]
OUT_DIR = REPO_ROOT / "benchmark_results" / "flagship_rps_sweep"

MODES = ["sync@10", "async-pull", "async-ded", "async-chan"]
# User-facing labels: sync / pull / dedicated / channel
MODE_LABELS = {
    "sync@10": "sync",
    "async-pull": "pull",  # Pull — shared disruptor cursor (CAS)
    "async-ded": "dedicated",  # PullDedicated — per-worker disruptor
    "async-chan": "channel",  # Channel — reader→channel fan-out
}
MODE_COLORS = {
    "sync@10": "#1f77b4",
    "async-pull": "#ff7f0e",
    "async-ded": "#2ca02c",
    "async-chan": "#d62728",
}
MODE_MARKERS = {
    "sync@10": "o",
    "async-pull": "s",
    "async-ded": "^",
    "async-chan": "D",
}

METRICS = [
    ("p50_us", "p50 latency (µs)"),
    ("p95_us", "p95 latency (µs)"),
    ("p99_us", "p99 latency (µs)"),
]


def load_results(directory: Path) -> dict[int, dict[str, dict]]:
    """
    Returns {effective_rps: {mode_name: result_dict}}.
    For steady: effective_rps = rps field.
    For burst:  effective_rps = burst_effective_rps field.
    """
    data: dict[int, dict[str, dict]] = {}
    for path in sorted(directory.glob("*.json")):
        with path.open() as f:
            blob = json.load(f)
        if blob["load"] == "steady":
            eff_rps = int(blob["rps"])
        else:
            eff_rps = int(blob["burst_effective_rps"])
        data[eff_rps] = {r["name"]: r for r in blob["results"]}
    return data


def format_latency(val: float) -> str:
    if val >= 1_000_000:
        return f"{val / 1e6:.1f}s"
    if val >= 1_000:
        return f"{val / 1e3:.1f}ms"
    if val >= 1:
        return f"{val:.1f}µs"
    return f"{val * 1000:.0f}ns"


def plot_sweep(
        title: str,
        subtitle: str,
        data: dict[int, dict[str, dict]],
        out_path: Path,
) -> None:
    rps_values = sorted(data.keys())
    fig, axes = plt.subplots(1, len(METRICS), figsize=(18, 5.5), sharey=False)
    fig.patch.set_facecolor("white")
    fig.suptitle(title, fontsize=14, fontweight="bold", y=1.02)
    fig.text(0.5, 0.975, subtitle, ha="center", fontsize=10, color="#555")

    for ax, (metric_key, metric_label) in zip(axes, METRICS):
        any_data = False
        for mode in MODES:
            xs, ys = [], []
            for rps in rps_values:
                rec = data[rps].get(mode)
                if rec:
                    val = rec.get(metric_key, 0) or 0
                    xs.append(rps)
                    ys.append(float(val))
            if not xs:
                continue
            ax.plot(
                xs, ys,
                marker=MODE_MARKERS[mode],
                markersize=5,
                linewidth=1.8,
                color=MODE_COLORS[mode],
                label=MODE_LABELS[mode],
            )
            any_data = True

        ax.set_title(metric_label, fontsize=11)
        ax.set_xlabel("Effective RPS", fontsize=9)
        ax.set_xscale("log")
        ax.set_xticks(rps_values)
        ax.xaxis.set_major_formatter(mticker.ScalarFormatter())
        ax.tick_params(axis="x", rotation=45)
        ax.grid(True, linestyle="--", alpha=0.35, zorder=0)
        ax.set_axisbelow(True)
        ax.yaxis.set_major_formatter(
            mticker.FuncFormatter(lambda v, _: format_latency(v))
        )
        if any_data:
            ax.set_yscale("log")

    handles, labels = axes[0].get_legend_handles_labels()
    fig.legend(
        handles, labels,
        loc="upper right",
        fontsize=10,
        ncol=1,
        frameon=True,
        framealpha=0.9,
        bbox_to_anchor=(1.0, 1.0),
    )

    plt.tight_layout(rect=(0, 0, 0.88, 0.95))
    out_path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(out_path, dpi=130, bbox_inches="tight", facecolor="white")
    plt.close(fig)
    print(f"Wrote {out_path}")


def main() -> None:
    steady_dir = SWEEP_DIR / "steady"
    burst_dir = SWEEP_DIR / "burst"

    if not steady_dir.exists():
        raise SystemExit(f"Missing {steady_dir} — run run_flagship_rps_sweep.sh first")
    if not burst_dir.exists():
        raise SystemExit(f"Missing {burst_dir} — run run_flagship_rps_sweep.sh first")

    steady_data = load_results(steady_dir)
    burst_data = load_results(burst_dir)

    print(f"Steady configs: {sorted(steady_data.keys())}")
    print(f"Burst effective RPS: {sorted(burst_data.keys())}")

    plot_sweep(
        title="Flagship — Steady arrival: latency vs RPS",
        subtitle="Uniform Poisson arrival  ·  pool=128  ·  work=ws  ·  10 shards  ·  8 workers/shard",
        data=steady_data,
        out_path=OUT_DIR / "steady_latency_vs_rps.png",
    )
    plot_sweep(
        title="Flagship — Burst arrival: latency vs burst intensity",
        subtitle="burst every 500ms  ·  effective_rps = burst_size × 2  ·  pool=128  ·  work=ws  ·  10 shards  ·  8 workers/shard",
        data=burst_data,
        out_path=OUT_DIR / "burst_latency_vs_rps.png",
    )


if __name__ == "__main__":
    main()
