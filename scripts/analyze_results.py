#!/usr/bin/env python3
"""Summarize AegisOS simulation CSVs and run paired bootstrap comparisons."""
import argparse
import csv
import math
import os
import random
import statistics
from collections import defaultdict

NUMERIC = [
    "foreground_mean_latency_ms", "foreground_p95_latency_ms", "deadline_miss_rate",
    "background_cpu_ms", "cpu_utilization", "controller_changes", "final_foreground_share",
    "background_deadline_miss_rate",
]
POLICIES = ("equal", "fixed_high_share", "foreground_priority", "background_cap", "adaptive")
CALIBRATION = ("steady", "sustained")
HELD_OUT = ("bursty", "mixed")
LATENCY_TARGET_MS = 8.0
BOOTSTRAP_SAMPLES = 10000


def mean(xs):
    return statistics.mean(xs) if xs else 0.0


def median(xs):
    return statistics.median(xs) if xs else 0.0


def percentile(xs, q):
    if not xs:
        return 0.0
    ys = sorted(xs)
    return ys[min(len(ys) - 1, max(0, math.ceil(q * len(ys)) - 1))]


def bootstrap_paired_ci(diffs, seed=20261021):
    """Percentile CI for paired run-level differences; pairing uses identical scenario seeds."""
    if not diffs:
        return (0.0, 0.0)
    if len(diffs) == 1:
        return (diffs[0], diffs[0])
    rng = random.Random(seed)
    n = len(diffs)
    boot = []
    for _ in range(BOOTSTRAP_SAMPLES):
        boot.append(mean([diffs[rng.randrange(n)] for _ in range(n)]))
    return percentile(boot, 0.025), percentile(boot, 0.975)


def load_rows(path):
    with open(path, newline="", encoding="utf-8") as f:
        rows = list(csv.DictReader(f))
    if not rows:
        raise SystemExit("Input CSV has no result rows")
    for row in rows:
        for key in NUMERIC:
            row[key] = float(row[key])
        for key in ("seed", "duration_ms", "generated_foreground_requests", "completed_foreground_requests",
                    "completed_background_units", "background_jobs_generated", "background_jobs_completed",
                    "background_jobs_deadline_missed", "background_jobs_on_time"):
            row[key] = int(row[key])
    return rows


def summarize(rows, outdir):
    groups = defaultdict(list)
    for row in rows:
        groups[(row["scenario"], row["policy"])].append(row)
    fields = ["scenario", "policy", "runs", "mean_foreground_p95_ms", "median_foreground_p95_ms",
              "bootstrap_ci95_p95_low", "bootstrap_ci95_p95_high", "mean_background_jobs_on_time",
              "mean_background_jobs_generated", "mean_background_jobs_completed", "mean_background_jobs_deadline_missed",
              "mean_background_deadline_miss_rate", "mean_background_cpu_ms", "mean_deadline_miss_rate", "mean_cpu_utilization", "mean_controller_changes"]
    summary = []
    for (scenario, policy), rs in sorted(groups.items()):
        p95 = [r["foreground_p95_latency_ms"] for r in rs]
        lo, hi = bootstrap_paired_ci(p95, seed=101 + sum(map(ord, scenario + policy)))
        summary.append({
            "scenario": scenario, "policy": policy, "runs": len(rs),
            "mean_foreground_p95_ms": f"{mean(p95):.4f}",
            "median_foreground_p95_ms": f"{median(p95):.4f}",
            "bootstrap_ci95_p95_low": f"{lo:.4f}", "bootstrap_ci95_p95_high": f"{hi:.4f}",
            "mean_background_jobs_on_time": f"{mean([r['background_jobs_on_time'] for r in rs]):.2f}",
            "mean_background_jobs_generated": f"{mean([r['background_jobs_generated'] for r in rs]):.2f}",
            "mean_background_jobs_completed": f"{mean([r['background_jobs_completed'] for r in rs]):.2f}",
            "mean_background_jobs_deadline_missed": f"{mean([r['background_jobs_deadline_missed'] for r in rs]):.2f}",
            "mean_background_deadline_miss_rate": f"{mean([r['background_deadline_miss_rate'] for r in rs]):.5f}",
            "mean_background_cpu_ms": f"{mean([r['background_cpu_ms'] for r in rs]):.2f}",
            "mean_deadline_miss_rate": f"{mean([r['deadline_miss_rate'] for r in rs]):.5f}",
            "mean_cpu_utilization": f"{mean([r['cpu_utilization'] for r in rs]):.5f}",
            "mean_controller_changes": f"{mean([r['controller_changes'] for r in rs]):.2f}",
        })
    with open(os.path.join(outdir, "summary.csv"), "w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fields); writer.writeheader(); writer.writerows(summary)
    return groups, summary


def compare_calibrated_static(groups):
    # Select one static policy using calibration scenarios only: satisfy p95 target, then maximize
    # on-time background completions. If none qualifies, minimize mean p95 first.
    candidates = []
    static = [p for p in POLICIES if p != "adaptive"]
    for policy in static:
        rs = [r for scenario in CALIBRATION for r in groups.get((scenario, policy), [])]
        if not rs:
            continue
        avg_p95 = mean([r["foreground_p95_latency_ms"] for r in rs])
        avg_bg_on_time = mean([r["background_jobs_on_time"] for r in rs])
        qualifies = avg_p95 <= LATENCY_TARGET_MS
        candidates.append((policy, qualifies, avg_p95, avg_bg_on_time))
    qualified = [c for c in candidates if c[1]]
    pool = qualified if qualified else candidates
    if not pool:
        return None
    if qualified:
        return sorted(pool, key=lambda c: (-c[3], c[2]))[0]
    return sorted(pool, key=lambda c: (c[2], -c[3]))[0]


def write_report(rows, groups, summary, outdir, input_path):
    chosen = compare_calibrated_static(groups)
    path = os.path.join(outdir, "REPORT.md")
    with open(path, "w", encoding="utf-8") as f:
        f.write("# AegisOS simulation results\n\n")
        f.write(f"Input: `{os.path.relpath(os.path.abspath(input_path), os.getcwd())}`. Runs analyzed: {len(rows)}.\n\n")
        f.write("These are results from the bundled simplified simulator, not measurements from a real Linux system. Confidence intervals are percentile bootstrap intervals for run-level p95 latency. They summarize variation across seeds; they do not correct for model limitations.\n\n")
        f.write(f"**Predefined foreground p95 target:** {LATENCY_TARGET_MS:.1f} ms. **Calibration scenarios:** {', '.join(CALIBRATION)}. **Held-out scenarios:** {', '.join(HELD_OUT)}.\n\n")
        if chosen:
            f.write(f"**Static policy selected using calibration scenarios only:** `{chosen[0]}` (mean p95 {chosen[2]:.2f} ms; mean on-time background completions {chosen[3]:.2f}). Selection rule: among static policies meeting the calibration p95 target, maximize on-time background completions.\n\n")
        f.write("## Summary\n\n")
        f.write("| Scenario | Policy | Runs | Mean p95 (ms) | Median p95 (ms) | Bootstrap 95% CI for mean p95 | BG jobs on time | BG deadline-miss rate | Mean CPU utilization | Controller changes |\n")
        f.write("|---|---|---:|---:|---:|---:|---:|---:|---:|---:|\n")
        for r in summary:
            f.write(f"| {r['scenario']} | {r['policy']} | {r['runs']} | {r['mean_foreground_p95_ms']} | {r['median_foreground_p95_ms']} | [{r['bootstrap_ci95_p95_low']}, {r['bootstrap_ci95_p95_high']}] | {r['mean_background_jobs_on_time']} | {r['mean_background_deadline_miss_rate']} | {r['mean_cpu_utilization']} | {r['mean_controller_changes']} |\n")
        f.write("\n## Adaptive policy versus calibrated static policy on held-out scenarios\n\n")
        if chosen:
            static_policy = chosen[0]
            f.write("Positive p95 difference means adaptive latency is worse; positive background-completion difference means adaptive completes more background jobs on time. Intervals are paired bootstrap CIs across matched seeds.\n\n")
            f.write("| Scenario | p95 difference (adaptive - static), ms | Paired bootstrap 95% CI | On-time BG completion difference | Paired bootstrap 95% CI |\n")
            f.write("|---|---:|---:|---:|---:|\n")
            for scenario in HELD_OUT:
                a = {r["seed"]: r for r in groups.get((scenario, "adaptive"), [])}
                b = {r["seed"]: r for r in groups.get((scenario, static_policy), [])}
                seeds = sorted(set(a) & set(b))
                if not seeds:
                    continue
                dp = [a[s]["foreground_p95_latency_ms"] - b[s]["foreground_p95_latency_ms"] for s in seeds]
                db = [a[s]["background_jobs_on_time"] - b[s]["background_jobs_on_time"] for s in seeds]
                p_lo, p_hi = bootstrap_paired_ci(dp, seed=303 + sum(map(ord, scenario)))
                b_lo, b_hi = bootstrap_paired_ci(db, seed=707 + sum(map(ord, scenario)))
                f.write(f"| {scenario} | {mean(dp):.2f} | [{p_lo:.2f}, {p_hi:.2f}] | {mean(db):.2f} | [{b_lo:.2f}, {b_hi:.2f}] |\n")
        f.write("\n## Held-out scenario decisions\n\n")
        f.write("Differences are adaptive minus static, paired by seed. Negative P95 and background deadline-miss-rate differences favor adaptive scheduling. Both metrics are reported; no arbitrary acceptable-loss threshold is assumed.\n\n")
        f.write("| Scenario | P95 difference (ms) | P95 95% CI | Background deadline-miss-rate difference | Background 95% CI | Result |\n")
        f.write("|---|---:|---:|---:|---:|---|\n")
        if chosen:
            static_policy = chosen[0]
            for scenario in HELD_OUT:
                a = {r["seed"]: r for r in groups.get((scenario, "adaptive"), [])}
                b = {r["seed"]: r for r in groups.get((scenario, static_policy), [])}
                seeds = sorted(set(a) & set(b))
                if not seeds:
                    f.write(f"| {scenario} | — | — | — | — | INCONCLUSIVE: no matched runs |\n")
                    continue
                dp = [a[x]["foreground_p95_latency_ms"] - b[x]["foreground_p95_latency_ms"] for x in seeds]
                db = [a[x]["background_deadline_miss_rate"] - b[x]["background_deadline_miss_rate"] for x in seeds]
                p_lo, p_hi = bootstrap_paired_ci(dp, seed=303 + sum(map(ord, scenario)))
                b_lo, b_hi = bootstrap_paired_ci(db, seed=707 + sum(map(ord, scenario)))
                if p_hi < 0 and b_hi <= 0:
                    result = "ACCEPT: both metrics favor adaptive"
                elif p_lo > 0 and b_lo >= 0:
                    result = "REJECT: P95 worsened without background benefit"
                else:
                    result = "INCONCLUSIVE: mixed trade-off or uncertainty"
                f.write(f"| {scenario} | {mean(dp):.2f} | [{p_lo:.2f}, {p_hi:.2f}] | {mean(db):.5f} | [{b_lo:.5f}, {b_hi:.5f}] | {result} |\n")
        f.write("\n## Interpretation cautions\n\n- Lower foreground p95 and foreground deadline-miss rate are better. Higher on-time background completions are better. Consider both metrics together.\n- `background_cap` is an intentionally strict, non-borrowing cap; it is not equivalent to Linux `cpu.weight`.\n- Static policy selection uses only the designated calibration scenarios. Bursty and mixed scenarios are held out for the final comparison.\n- Do not claim adaptive superiority unless the held-out measurements support it. This simulator models one CPU in 1 ms steps and omits hardware, cache, I/O, kernel, and multicore effects.\n")
    return path


def plots(summary, outdir):
    try:
        import matplotlib.pyplot as plt
    except ImportError:
        print("matplotlib not installed; skipped plots")
        return
    scenarios = sorted({r["scenario"] for r in summary})
    policies = sorted({r["policy"] for r in summary})
    for metric, ylabel, filename in [
        ("mean_foreground_p95_ms", "Mean run-level foreground p95 latency (ms)", "foreground_p95.png"),
        ("mean_background_jobs_on_time", "Mean background jobs completed before deadline", "background_jobs_on_time.png"),
    ]:
        fig, ax = plt.subplots(figsize=(10, 5))
        for policy in policies:
            subset = [r for r in summary if r["policy"] == policy]
            lookup = {r["scenario"]: float(r[metric]) for r in subset}
            ax.plot(scenarios, [lookup.get(s, 0) for s in scenarios], marker="o", label=policy)
        ax.set_ylabel(ylabel); ax.set_xlabel("Scenario"); ax.legend(); ax.grid(True, alpha=0.3)
        fig.tight_layout(); fig.savefig(os.path.join(outdir, filename), dpi=150); plt.close(fig)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("csv_path")
    ap.add_argument("--out", default="results/analysis")
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)
    rows = load_rows(args.csv_path)
    groups, summary = summarize(rows, args.out)
    report = write_report(rows, groups, summary, args.out, args.csv_path)
    print(f"Wrote {os.path.join(args.out, 'summary.csv')}")
    print(f"Wrote {report}")
    plots(summary, args.out)

if __name__ == "__main__":
    main()
