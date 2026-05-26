#!/usr/bin/env python3
"""
Plot ResourcePool benchmark results: per-(scenario, worker-count) histograms AND
sync-vs-async scaling line plots, across every metric the benchmarks report.

The sync ResourcePool subjects (RP_*) and the async AsyncResourcePool subjects
(ARP_AcqFor_*) are plotted together so sync and async sit side by side in every
shared scenario (Steady / Burst / TimeoutPressure).

Input:  /tmp/pool_bench_results/{floating,pinned}/{try,acqfor_1us,acqfor_2us,
        acqfor_10us,rp_pinned,mutex_base,arp_acqfor_1us,arp_acqfor_2us,
        arp_acqfor_10us}.json
        (Google Benchmark JSON output, one file per subject binary per variant)

Output: benchmark_results/pool_histograms/{floating,pinned}/<scenario>_w<workers>.png
            One figure per (variant, scenario, worker-count); one subplot per metric,
            one bar per subject that registered that scenario.
        benchmark_results/pool_histograms/floating/scaling_<scenario>.png
            One figure per shared scenario; metric vs worker-count, one line per
            subject — this is the sync-vs-async scaling comparison.

Run:    python3 scripts/plot_pool_bench_histograms.py
"""

import json
from pathlib import Path

import matplotlib.pyplot as plt
import matplotlib.ticker as mticker

REPO_ROOT = Path(__file__).resolve().parents[4]
RESULTS_DIR = Path("/tmp/pool_bench_results")
OUT_DIR = REPO_ROOT / "benchmark_results" / "pool_histograms"

JSON_FILES = {
    "RP_Try": "try.json",
    "RP_AcqFor_1us": "acqfor_1us.json",
    "RP_AcqFor_2us": "acqfor_2us.json",
    "RP_AcqFor_10us": "acqfor_10us.json",
    "RP_Pinned": "rp_pinned.json",
    "ARP_AcqFor_1us": "arp_acqfor_1us.json",
    "ARP_AcqFor_2us": "arp_acqfor_2us.json",
    "ARP_AcqFor_10us": "arp_acqfor_10us.json",
}
SUBJECTS = list(JSON_FILES.keys())

# Stable per-subject color map. Sync RP_* in cool hues; async ARP_* in warm hues
# so the sync-vs-async pairs are easy to tell apart in shared plots.
COLORS = {
    "RP_Try": "#1f77b4",
    "RP_AcqFor_1us": "#17becf",
    "RP_AcqFor_2us": "#2ca02c",
    "RP_AcqFor_10us": "#9467bd",
    "RP_Pinned": "#8c564b",
    "ARP_AcqFor_1us": "#ff7f0e",
    "ARP_AcqFor_2us": "#e377c2",
    "ARP_AcqFor_10us": "#bcbd22",
}

SCENARIOS = [
    "Steady",
    "Burst",
    "TimeoutPressure",
    "AsioPostSteady",
    "AsioPostBurst",
    "PinnedZeroContention",
    "HeavyBurst",
]

# Which scenarios each subject actually registers — used to suppress spurious
# "missing" warnings (async subjects skip AsioPost*/Pinned; pinned/try are
# scenario-specific). A subject is only "missing" if it should have run.
_FREE5 = {"Steady", "Burst", "TimeoutPressure", "AsioPostSteady", "AsioPostBurst", "HeavyBurst"}
_ASYNC3 = {"Steady", "Burst", "TimeoutPressure", "HeavyBurst"}
SUBJECT_SCENARIOS = {
    "RP_Try": _FREE5,
    "RP_AcqFor_1us": _FREE5,
    "RP_AcqFor_2us": _FREE5,
    "RP_AcqFor_10us": _FREE5,
    "RP_Pinned": {"PinnedZeroContention"},
    "ARP_AcqFor_1us": _ASYNC3,
    "ARP_AcqFor_2us": _ASYNC3,
    "ARP_AcqFor_10us": _ASYNC3,
}

VARIANT_WORKERS = {
    "floating": [8, 16, 32, 64, 128, 256],
    "pinned": [10],
}

# Scenario parameters baked into bench_scenarios.hpp. Used for subtitles.
SCENARIO_PARAMS = {
    "Steady": "W=500ns  P=128",
    "Burst": "W=500ns  B=8   I=10µs  P=128",
    "TimeoutPressure": "W=1µs    P=128",
    "AsioPostSteady": "1 producer → asio[N] → RP    W=500ns  P=128",
    "AsioPostBurst": "1 producer → asio[N] burst=8 idle=10µs → RP    P=128",
    "PinnedZeroContention": "W=0      K=workers (1 pinned cell per worker; floor reference)",
    "HeavyBurst": "W=10µs  B=256  I=500µs  P=128  (slot held across async I/O → pool saturates/parks)",
}

METRICS = [
    ("items_per_second", "items / sec", False),
    ("p50_us", "p50 latency (µs)", True),
    ("p95_us", "p95 latency (µs)", True),
    ("p99_us", "p99 latency (µs)", True),
    ("skipped", "skipped acquires (count)", False),
]


def parse_name(name):
    """'BM_RP_Try_Steady/8/real_time' -> ('RP_Try', 'Steady', 8)."""
    base, workers_str, _ = name.split("/")
    # base is "BM_<Subject>_<Scenario>". The Subject can have underscores
    # and identify the subject by longest-prefix match.
    body = base[3:]  # drop "BM_"
    for subj in sorted(SUBJECTS, key=len, reverse=True):
        if body.startswith(subj + "_"):
            return subj, body[len(subj) + 1:], int(workers_str)
    raise ValueError(f"could not parse subject from benchmark name: {name}")


def load_variant(variant_dir):
    """data[scenario][workers][subject] = full benchmark record."""
    data = {}
    for subject, fname in JSON_FILES.items():
        path = variant_dir / fname
        if not path.exists():
            print(f"  SKIP missing file {path}")
            continue
        with path.open() as f:
            blob = json.load(f)
        for bench in blob.get("benchmarks", []):
            try:
                subj, scenario, workers = parse_name(bench["name"])
            except ValueError as e:
                print(f"  WARN {e}")
                continue
            if subj != subject:
                print(f"  WARN {fname} emitted unexpected subject {subj} in {bench['name']}")
                continue
            data.setdefault(scenario, {}).setdefault(workers, {})[subject] = bench
    return data


def format_label(val, metric_key):
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
    if val >= 1:
        return f"{val:.2f}µs"
    return f"{val * 1000:.0f}ns"


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


def extract_pinned_floor(data):
    """Return {workers: p50_us} for the PinnedZeroContention scenario."""
    floor = {}
    for workers, per_subject in data.get("PinnedZeroContention", {}).items():
        rec = per_subject.get("RP_Pinned")
        if rec is None:
            continue
        floor[workers] = rec.get("p50_us", 0.0)
    return floor


def plot_one(variant, scenario, workers, per_subject, out_dir, pinned_floor_us):
    fig, axes = plt.subplots(1, len(METRICS), figsize=(22, 4.8))
    fig.patch.set_facecolor("white")

    params = SCENARIO_PARAMS.get(scenario, "")
    rps_ceiling_vals = [rec.get("rps_ceiling", 0) for rec in per_subject.values() if rec]
    rps_ceiling = max(rps_ceiling_vals) if rps_ceiling_vals else 0
    ceiling_str = (
        f"rps_ceiling={rps_ceiling:,.0f}/s" if rps_ceiling else "rps_ceiling=n/a"
    )

    subtitle = (
        f"{params}   ·   {workers} workers   ·   {variant}   ·   {ceiling_str}"
    )
    fig.suptitle(scenario, fontsize=15, fontweight="bold", y=1.02)
    fig.text(0.5, 0.965, subtitle, ha="center", fontsize=10, color="#555")

    # Subjects actually present for this (scenario, workers).
    present_subjects = [s for s in SUBJECTS if s in per_subject]

    for ax, (metric_key, metric_label, use_log) in zip(axes, METRICS):
        vals = [float(per_subject[s].get(metric_key, 0) or 0) for s in present_subjects]

        bars = ax.bar(
            present_subjects,
            vals,
            color=[COLORS[s] for s in present_subjects],
            edgecolor="#222",
            linewidth=0.5,
        )

        ax.set_title(metric_label, fontsize=11)
        ax.grid(axis="y", linestyle="--", alpha=0.35, zorder=0)
        ax.set_axisbelow(True)
        plt.setp(ax.get_xticklabels(), rotation=40, ha="right",
                 rotation_mode="anchor", fontsize=7)
        ax.yaxis.set_major_formatter(mticker.FuncFormatter(format_axis_value))

        if use_log and any(v > 0 for v in vals):
            ax.set_yscale("log")
            nonzero = [v for v in vals if v > 0]
            if nonzero:
                ax.set_ylim(bottom=max(0.001, min(nonzero) * 0.5))

        if metric_key == "items_per_second" and rps_ceiling:
            ax.axhline(rps_ceiling, color="#666", linestyle=":", linewidth=1.2,
                       label=f"ceiling {rps_ceiling:,.0f}")
            ax.legend(loc="upper right", fontsize=8, frameon=False)

        # Overlay pinned floor as horizontal reference on latency subplots.
        if (metric_key in ("p50_us", "p95_us", "p99_us")
                and pinned_floor_us is not None
                and pinned_floor_us > 0
                and scenario != "PinnedZeroContention"):
            ax.axhline(pinned_floor_us, color="#8c564b", linestyle="--",
                       linewidth=1.0, alpha=0.7,
                       label=f"pinned floor {format_label(pinned_floor_us, metric_key)}")
            ax.legend(loc="upper left", fontsize=7, frameon=False)

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
        if ymax == 0:
            ax.text(0.5, 0.5, "all zero", ha="center", va="center",
                    transform=ax.transAxes, color="#aaa", fontsize=10)

    plt.tight_layout(rect=(0, 0, 1, 0.95))
    out_path = out_dir / f"{scenario}_w{workers:04d}.png"
    fig.savefig(out_path, dpi=110, bbox_inches="tight", facecolor="white")
    plt.close(fig)
    return out_path


def plot_scaling(scenario, data, workers_list, out_dir):
    """Line plot: each metric vs worker count, one line per subject — the
    sync-vs-async scaling comparison. Only meaningful when there are several
    worker counts (the floating variant)."""
    present = [s for s in SUBJECTS
               if any(s in data[scenario].get(w, {}) for w in workers_list)]
    if not present:
        return None

    fig, axes = plt.subplots(1, len(METRICS), figsize=(22, 4.8))
    fig.patch.set_facecolor("white")
    fig.suptitle(f"{scenario} — scaling vs worker count",
                 fontsize=15, fontweight="bold", y=1.04)
    params = SCENARIO_PARAMS.get(scenario, "")
    fig.text(0.5, 0.965, f"{params}   ·   floating   ·   sync RP_* vs async ARP_*",
             ha="center", fontsize=10, color="#555")

    for ax, (metric_key, metric_label, use_log) in zip(axes, METRICS):
        plotted_any = False
        for s in present:
            xs, ys = [], []
            for w in workers_list:
                rec = data[scenario].get(w, {}).get(s)
                if rec is None:
                    continue
                xs.append(w)
                ys.append(float(rec.get(metric_key, 0) or 0))
            if not xs:
                continue
            ax.plot(xs, ys, marker="o", markersize=4, linewidth=1.6,
                    color=COLORS[s], label=s)
            plotted_any = True

        ax.set_title(metric_label, fontsize=11)
        ax.set_xlabel("workers", fontsize=9)
        ax.set_xscale("log", base=2)
        ax.set_xticks(workers_list)
        ax.xaxis.set_major_formatter(mticker.ScalarFormatter())
        ax.grid(True, linestyle="--", alpha=0.35, zorder=0)
        ax.set_axisbelow(True)
        ax.yaxis.set_major_formatter(mticker.FuncFormatter(format_axis_value))
        if use_log and plotted_any:
            ax.set_yscale("log")

    handles, labels = axes[0].get_legend_handles_labels()
    fig.legend(handles, labels, loc="upper right", fontsize=8, ncol=2, frameon=False)
    plt.tight_layout(rect=(0, 0, 1, 0.92))
    out_path = out_dir / f"scaling_{scenario}.png"
    fig.savefig(out_path, dpi=110, bbox_inches="tight", facecolor="white")
    plt.close(fig)
    return out_path


def main():
    total = 0
    for variant, workers_list in VARIANT_WORKERS.items():
        variant_dir = RESULTS_DIR / variant
        out_dir = OUT_DIR / variant
        out_dir.mkdir(parents=True, exist_ok=True)

        if not variant_dir.exists():
            print(f"SKIP variant {variant}: {variant_dir} missing")
            continue

        print(f"=== {variant} ===")
        data = load_variant(variant_dir)
        pinned_floor = extract_pinned_floor(data)
        for scenario in SCENARIOS:
            if scenario not in data:
                print(f"  SKIP scenario {scenario}: no data in {variant}")
                continue
            for workers in workers_list:
                if workers not in data[scenario]:
                    print(f"  SKIP {scenario}/{workers}: no data in {variant}")
                    continue
                per_subject = data[scenario][workers]
                missing = [s for s in SUBJECTS
                           if scenario in SUBJECT_SCENARIOS[s] and s not in per_subject]
                if missing:
                    print(f"  WARN {variant}/{scenario}/{workers}: missing {missing}")
                # Match the floor at the same worker count if available;
                # otherwise fall back to the closest.
                floor_us = pinned_floor.get(workers)
                if floor_us is None and pinned_floor:
                    closest = min(pinned_floor.keys(),
                                  key=lambda w: abs(w - workers))
                    floor_us = pinned_floor[closest]
                out_path = plot_one(variant, scenario, workers, per_subject,
                                    out_dir, floor_us)
                total += 1
                print(f"  wrote {out_path.relative_to(REPO_ROOT)}")

        # Sync-vs-async scaling line plots (needs several worker counts → floating).
        if len(workers_list) > 1:
            for scenario in SCENARIOS:
                if scenario not in data:
                    continue
                scaling_path = plot_scaling(scenario, data, workers_list, out_dir)
                if scaling_path is not None:
                    total += 1
                    print(f"  wrote {scaling_path.relative_to(REPO_ROOT)}")

    print(f"\nGenerated {total} figures under {OUT_DIR.relative_to(REPO_ROOT)}")


if __name__ == "__main__":
    main()
