#!/usr/bin/env python3
"""
Plot per-(scenario, worker-count) histograms comparing all 5 benchmark subjects
across every metric reported by the PostgreSQL concurrency-scenario benchmarks.

Input:  /tmp/bench_results/{libpq,pqxx,sync_executor,lock_free,blocking}.json
        (Google Benchmark JSON output, one file per subject binary)

Output: benchmark_results/histograms/<scenario>_w<workers>.png
        One figure per (scenario, worker-count) pair. Each figure has one
        subplot per metric; each subplot shows 5 bars (one per subject).

Run:    python3 scripts/plot_bench_histograms.py
"""

import json
import matplotlib.pyplot as plt
import matplotlib.ticker as mticker
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
RESULTS_DIR = Path("/tmp/bench_results")
OUT_DIR = REPO_ROOT / "benchmark_results" / "histograms"
OUT_DIR.mkdir(parents=True, exist_ok=True)

# Subject binaries and their display names, in fixed order so bar ordering is
# deterministic across every figure.
JSON_FILES = {
    "LibPQ": "libpq.json",
    "Pqxx": "pqxx.json",
    "SyncExecutor": "sync_executor.json",
    "LockFree": "lock_free.json",
    "Blocking": "blocking.json",
}
SUBJECTS = list(JSON_FILES.keys())

# Stable per-subject color map. Control group = cool tones, session group = warm.
COLORS = {
    "LibPQ": "#1f77b4",
    "Pqxx": "#17becf",
    "SyncExecutor": "#2ca02c",
    "LockFree": "#ff7f0e",
    "Blocking": "#d62728",
}

SCENARIOS = [
    "OverheadBaseline",
    "SaturatedSteady",
    "BurstFireshot",
    "Oversubscribed",
    "UnderProvisioned",
]

WORKERS = [8, 16, 32, 64, 128, 256, 512]

# Scenario parameters baked into bench_scenarios.hpp. Used for subtitles.
SCENARIO_PARAMS = {
    "OverheadBaseline": {"Q": 1, "D": "instant", "E": 8, "C": 8},
    "SaturatedSteady": {"Q": 1, "D": "10ms", "E": 8, "C": 8},
    "BurstFireshot": {"Q": 4, "D": "10ms", "E": 8, "C": 8},
    "Oversubscribed": {"Q": 1, "D": "10ms", "E": 4, "C": 16},
    "UnderProvisioned": {"Q": 1, "D": "10ms", "E": 16, "C": 4},
}

# (metric-key, display-label, log-scale-for-varying-magnitudes)
METRICS = [
    ("items_per_second", "items / sec", False),
    ("p50_us", "p50 latency (µs)", True),
    ("p95_us", "p95 latency (µs)", True),
    ("p99_us", "p99 latency (µs)", True),
    ("skipped", "skipped acquires (count)", False),
]


def parse_name(name):
    """'BM_LibPQ_OverheadBaseline/8/real_time' -> ('LibPQ', 'OverheadBaseline', 8)."""
    base, workers_str, _ = name.split("/")
    parts = base.split("_", 2)  # ['BM', '<subject>', '<scenario>']
    return parts[1], parts[2], int(workers_str)


def load_all():
    """data[scenario][workers][subject] = full benchmark record."""
    data = {}
    for subject, fname in JSON_FILES.items():
        path = RESULTS_DIR / fname
        with path.open() as f:
            blob = json.load(f)
        for bench in blob["benchmarks"]:
            subj, scenario, workers = parse_name(bench["name"])
            if subj != subject:
                # Cross-contamination safety: binary emitted a benchmark with a
                # name that doesn't match its own subject. Skip it loudly.
                print(f"WARN: {fname} contains unexpected subject {subj} in {bench['name']}")
                continue
            data.setdefault(scenario, {}).setdefault(workers, {})[subject] = bench
    return data


def format_label(val, metric_key):
    """Short human-readable value label for bar annotations."""
    if val is None:
        return ""
    if val == 0:
        return "0"
    if metric_key in ("items_per_second", "skipped"):
        if val >= 1_000_000:
            return f"{val / 1e6:.2f}M"
        if val >= 1_000:
            return f"{val / 1e3:.1f}k"
        return f"{val:.0f}"
    # latency in µs
    if val >= 1_000_000:
        return f"{val / 1e6:.1f}s"
    if val >= 1_000:
        return f"{val / 1e3:.1f}ms"
    return f"{val:.0f}µs"


def format_axis_value(val, _pos):
    if val == 0:
        return "0"
    absv = abs(val)
    if absv >= 1_000_000:
        return f"{val / 1e6:.1f}M"
    if absv >= 1_000:
        return f"{val / 1e3:.0f}k"
    if absv >= 1:
        return f"{val:.0f}"
    return f"{val:.2f}"


def plot_one(scenario, workers, per_subject):
    fig, axes = plt.subplots(1, len(METRICS), figsize=(22, 4.8))
    fig.patch.set_facecolor("white")

    params = SCENARIO_PARAMS[scenario]
    rps_ceiling_vals = [
        rec.get("rps_ceiling", 0) for rec in per_subject.values() if rec
    ]
    rps_ceiling = rps_ceiling_vals[0] if rps_ceiling_vals else 0
    ceiling_str = (
        f"rps_ceiling={rps_ceiling:,.0f}/s" if rps_ceiling else "rps_ceiling=instant"
    )

    subtitle = (
        f"Q={params['Q']}  D={params['D']}  E={params['E']}  C={params['C']}"
        f"   ·   {workers} workers   ·   {ceiling_str}"
    )
    fig.suptitle(
        f"{scenario}",
        fontsize=15,
        fontweight="bold",
        y=1.02,
    )
    fig.text(0.5, 0.965, subtitle, ha="center", fontsize=10, color="#555")

    for ax, (metric_key, metric_label, use_log) in zip(axes, METRICS):
        vals = []
        for s in SUBJECTS:
            rec = per_subject.get(s)
            v = rec.get(metric_key, 0) if rec else 0
            if v is None:
                v = 0
            vals.append(float(v))

        bars = ax.bar(
            SUBJECTS,
            vals,
            color=[COLORS[s] for s in SUBJECTS],
            edgecolor="#222",
            linewidth=0.5,
        )

        ax.set_title(metric_label, fontsize=11)
        ax.grid(axis="y", linestyle="--", alpha=0.35, zorder=0)
        ax.set_axisbelow(True)
        ax.tick_params(axis="x", rotation=30, labelsize=9)
        ax.yaxis.set_major_formatter(mticker.FuncFormatter(format_axis_value))

        if use_log and any(v > 0 for v in vals):
            ax.set_yscale("log")
            # Ensure the y-range shows small values too.
            nonzero = [v for v in vals if v > 0]
            if nonzero:
                ax.set_ylim(bottom=max(1.0, min(nonzero) * 0.5))

        # rps_ceiling reference line on the items_per_second plot.
        if metric_key == "items_per_second" and rps_ceiling:
            ax.axhline(
                rps_ceiling,
                color="#666",
                linestyle=":",
                linewidth=1.2,
                label=f"ceiling {rps_ceiling:,.0f}",
            )
            ax.legend(loc="upper right", fontsize=8, frameon=False)

        # Bar value annotations.
        ymax = max(vals) if vals else 0
        for bar, v in zip(bars, vals):
            y = bar.get_height()
            if y <= 0:
                continue
            ax.annotate(
                format_label(v, metric_key),
                (bar.get_x() + bar.get_width() / 2, y),
                xytext=(0, 3),
                textcoords="offset points",
                ha="center",
                fontsize=8,
                color="#222",
            )
        # If all zero, show a placeholder.
        if ymax == 0:
            ax.text(
                0.5, 0.5, "all zero", ha="center", va="center",
                transform=ax.transAxes, color="#aaa", fontsize=10,
            )

    plt.tight_layout(rect=(0, 0, 1, 0.95))
    out_path = OUT_DIR / f"{scenario}_w{workers:04d}.png"
    fig.savefig(out_path, dpi=110, bbox_inches="tight", facecolor="white")
    plt.close(fig)
    return out_path


def main():
    data = load_all()
    count = 0
    for scenario in SCENARIOS:
        if scenario not in data:
            print(f"SKIP scenario {scenario}: no data")
            continue
        for workers in WORKERS:
            if workers not in data[scenario]:
                print(f"SKIP {scenario}/{workers}: no data")
                continue
            per_subject = data[scenario][workers]
            missing = [s for s in SUBJECTS if s not in per_subject]
            if missing:
                print(f"WARN {scenario}/{workers}: missing subjects {missing}")
            out_path = plot_one(scenario, workers, per_subject)
            count += 1
            print(f"  wrote {out_path.relative_to(REPO_ROOT)}")
    print(f"\nGenerated {count} histograms in {OUT_DIR.relative_to(REPO_ROOT)}")


if __name__ == "__main__":
    main()
