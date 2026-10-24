
#!/usr/bin/env python3
"""Evaluate AegisOS with foreground P95 latency as the primary objective."""

import argparse
import json
import math
from datetime import datetime
from pathlib import Path

DEFAULT_POLICY = {
    "schema_version": 1,
    "status": "accepted_default",
    "policy_name": "latency_formula_v1",
    "formula": {
        "name": "latency_ratio_linear_clamped",
        "target_latency_ms": 8.0,
        "base_share": 0.50,
        "latency_coefficient": 0.20,
        "minimum_foreground_share": 0.55,
        "maximum_foreground_share": 0.90,
        "total_weight_budget": 1000,
        "round_to_nearest": 10,
    },
    "validation": {
        "status": "not_yet_validated",
        "note": "Default parameters; validation applies only to the tested workload.",
    },
}

T_CRIT_95 = {
    1: 12.706, 2: 4.303, 3: 3.182, 4: 2.776, 5: 2.571,
    6: 2.447, 7: 2.365, 8: 2.306, 9: 2.262, 10: 2.228,
    11: 2.201, 12: 2.179, 13: 2.160, 14: 2.145, 15: 2.131,
    16: 2.120, 17: 2.110, 18: 2.101, 19: 2.093, 20: 2.086,
    21: 2.080, 22: 2.074, 23: 2.069, 24: 2.064, 25: 2.060,
    26: 2.056, 27: 2.052, 28: 2.048, 29: 2.045, 30: 2.042,
}


def mean_var(values):
    mean = sum(values) / len(values)
    if len(values) < 2:
        return mean, None
    variance = sum((x - mean) ** 2 for x in values) / (len(values) - 1)
    return mean, variance


def t_critical(df):
    if not math.isfinite(df) or df < 1:
        return None
    if df > 30:
        return 1.96
    return T_CRIT_95[max(1, math.ceil(df))]


def welch_difference_ci(a, b):
    """Approximate 95% CI for mean(a) - mean(b) across independent runs."""
    if len(a) < 2 or len(b) < 2:
        return {
            "difference": None, "low": None, "high": None,
            "method": "At least two independent runs per condition are required",
        }

    ma, va = mean_var(a)
    mb, vb = mean_var(b)
    term_a = va / len(a)
    term_b = vb / len(b)
    se2 = term_a + term_b
    se = math.sqrt(se2)

    denom = term_a**2 / (len(a) - 1) + term_b**2 / (len(b) - 1)
    df = se2**2 / denom if denom > 0 else float("inf")
    critical = t_critical(df)

    if critical is None:
        low = high = None
    else:
        half_width = critical * se
        low = ma - mb - half_width
        high = ma - mb + half_width

    return {
        "difference": ma - mb,
        "low": low,
        "high": high,
        "confidence_level": 0.95,
        "method": "Welch two-sample t interval on independent run-level summaries",
        "degrees_of_freedom_approx": df,
    }


def extract_run_values(results, group, field):
    try:
        values = [float(run[field]) for run in results["runs"][group]]
    except (KeyError, TypeError, ValueError) as exc:
        raise ValueError(
            f"Missing run-level field '{field}' for '{group}'. "
            "Check comparison_results.json."
        ) from exc

    if len(values) < 2:
        raise ValueError(f"At least two runs are required for '{group}'.")
    return values


def evaluate(results):
    baseline = results["baseline"]
    adaptive = results["adaptive"]

    # Positive improvement means baseline P95 minus adaptive P95 is positive.
    baseline_p95 = extract_run_values(results, "selected_fixed", "p95_ms")
    adaptive_p95 = extract_run_values(results, "adaptive", "p95_ms")
    p95_ci = welch_difference_ci(baseline_p95, adaptive_p95)

    # Positive difference means more background work under adaptive scheduling.
    baseline_bg = extract_run_values(
        results, "selected_fixed", "background_chunks_completed"
    )
    adaptive_bg = extract_run_values(
        results, "adaptive", "background_chunks_completed"
    )
    bg_ci = welch_difference_ci(adaptive_bg, baseline_bg)

    p95_improvement_pct = (
        (baseline["p95_ms"] - adaptive["p95_ms"])
        / baseline["p95_ms"] * 100
        if baseline["p95_ms"] > 0 else None
    )

    bg_baseline_mean = sum(baseline_bg) / len(baseline_bg)
    bg_adaptive_mean = sum(adaptive_bg) / len(adaptive_bg)
    bg_change_pct = (
        (bg_adaptive_mean - bg_baseline_mean) / bg_baseline_mean * 100
        if bg_baseline_mean > 0 else None
    )

    enough_data = p95_ci["low"] is not None and bg_ci["low"] is not None
    p95_improvement_supported = enough_data and p95_ci["low"] > 0
    p95_regression_supported = enough_data and p95_ci["high"] <= 0
    background_regression_supported = enough_data and bg_ci["high"] < 0

    # P95 is the primary objective. Background work is reported separately;
    # no arbitrary percentage-loss threshold is imposed.
    if not enough_data:
        decision = "INCONCLUSIVE"
        reason = "Insufficient independent run-level data to calculate confidence intervals."
    elif p95_improvement_supported:
        decision = "ACCEPT"
        reason = (
            "The 95% confidence interval supports lower foreground P95 latency "
            "for the tested workload. Background progress is reported separately."
        )
    elif p95_regression_supported:
        decision = "REJECT"
        reason = (
            "The 95% confidence interval does not support a positive P95 latency "
            "improvement; it indicates no improvement or worse P95 latency."
        )
    else:
        decision = "INCONCLUSIVE"
        reason = (
            "The 95% confidence interval does not establish a P95 latency "
            "improvement or regression. More independent runs may improve precision."
        )

    return {
        "decision": decision,
        "reason": reason,
        "criteria": {
            "primary_metric": "Foreground P95 probe latency",
            "baseline_p95_ms": baseline["p95_ms"],
            "adaptive_p95_ms": adaptive["p95_ms"],
            "p95_improvement_pct_descriptive": p95_improvement_pct,
            "baseline_run_p95_ms": baseline_p95,
            "adaptive_run_p95_ms": adaptive_p95,
            "p95_improvement_95ci_ms": p95_ci,
            "background_baseline_chunks_per_run": bg_baseline_mean,
            "background_adaptive_chunks_per_run": bg_adaptive_mean,
            "background_change_pct_descriptive": bg_change_pct,
            "adaptive_minus_baseline_background_chunks_95ci": bg_ci,
            "background_regression_supported": background_regression_supported,
        },
        "decision_method": {
            "primary_objective": (
                "Accept when the 95% confidence interval for baseline-minus-adaptive "
                "run-level P95 latency is wholly above zero."
            ),
            "background_tradeoff": (
                "Report background progress and its confidence interval separately. "
                "No claim of unchanged background progress is made."
            ),
            "limitations": (
                "The decision applies only to the tested workload. A practical "
                "background-loss limit must be specified from workload requirements "
                "before using one as an acceptance condition."
            ),
        },
        "limits": {
            "scope": "Tested synthetic foreground probe under background CPU contention",
            "not_a_general_performance_guarantee": True,
            "statistical_note": (
                "Approximate Welch intervals use independent run-level summaries; "
                "five runs per condition may provide limited precision."
            ),
        },
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("results", type=Path, help="comparison_results.json")
    parser.add_argument("--policy-path", type=Path, required=True)
    parser.add_argument("--evaluation-path", type=Path, required=True)
    # Retain compatibility with the existing caller.
    parser.add_argument("--min-latency-improvement", type=float, default=None,
                        help=argparse.SUPPRESS)
    parser.add_argument("--min-background-retention", type=float, default=None,
                        help=argparse.SUPPRESS)
    args = parser.parse_args()

    results = json.loads(args.results.read_text(encoding="utf-8"))
    decision = evaluate(results)
    decision["evaluated_at"] = datetime.now().astimezone().isoformat(timespec="seconds")
    decision["source_results"] = args.results.name
    args.evaluation_path.parent.mkdir(parents=True, exist_ok=True)

    if decision["decision"] == "ACCEPT":
        policy = dict(DEFAULT_POLICY)
        policy["formula"] = {
            **DEFAULT_POLICY["formula"],
            **results.get("policy_parameters_used", {}),
        }
        policy["status"] = "accepted"
        policy["validation"] = {
            "status": "validated_for_tested_workload",
            "evaluated_at": decision["evaluated_at"],
            "source_results": args.results.name,
            "criteria": decision["criteria"],
            "scope": decision["limits"]["scope"],
        }
        args.policy_path.parent.mkdir(parents=True, exist_ok=True)
        args.policy_path.write_text(json.dumps(policy, indent=2), encoding="utf-8")
        decision["policy_action"] = (
            "Policy parameters promoted for the tested workload only."
        )
    elif args.policy_path.exists():
        decision["policy_action"] = (
            "Previous selected policy retained; candidate was not promoted."
        )
    else:
        decision["policy_action"] = (
            "No policy promoted; documented defaults remain in use."
        )

    args.evaluation_path.write_text(json.dumps(decision, indent=2), encoding="utf-8")
    print(f"Policy decision: {decision['decision']} — {decision['reason']}")
    print(f"Evaluation: {args.evaluation_path}")
    print(f"Selected policy: {args.policy_path if args.policy_path.exists() else 'none (defaults in use)'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
