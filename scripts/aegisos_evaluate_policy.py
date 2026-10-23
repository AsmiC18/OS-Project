#!/usr/bin/env python3
"""Evaluate live AegisOS comparison results and select/retain a policy explicitly."""
import argparse
import json
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
        "round_to_nearest": 10
    },
    "validation": {"status": "not_yet_validated", "note": "Default parameters; collect live evaluation evidence before claiming they are validated."}
}


def evaluate(results, min_latency_improvement_pct=10.0, min_background_retention_pct=95.0):
    base = results["baseline"]
    adaptive = results["adaptive"]
    bg = results["background_chunks_completed_per_run"]
    p95_improvement = ((base["p95_ms"] - adaptive["p95_ms"]) / base["p95_ms"] * 100.0) if base["p95_ms"] > 0 else 0.0
    bg_retention = (bg["adaptive_mean"] / bg["baseline_mean"] * 100.0) if bg["baseline_mean"] > 0 else 0.0
    ci = results.get("mean_latency_reduction_95ci_ms", {})
    ci_low = ci.get("low")
    criteria = {
        "p95_latency_improvement_pct": p95_improvement,
        "minimum_p95_improvement_pct": min_latency_improvement_pct,
        "background_retention_pct": bg_retention,
        "minimum_background_retention_pct": min_background_retention_pct,
        "mean_latency_reduction_ci_low_ms": ci_low,
        "confidence_interval_excludes_zero": ci_low is not None and ci_low > 0,
        "p95_requirement_met": p95_improvement >= min_latency_improvement_pct,
        "background_requirement_met": bg_retention >= min_background_retention_pct,
    }
    if not criteria["p95_requirement_met"] or not criteria["background_requirement_met"]:
        decision = "REJECT"
        reason = "Adaptive policy did not meet the configured p95-latency and/or background-progress acceptance thresholds."
    elif not criteria["confidence_interval_excludes_zero"]:
        decision = "INCONCLUSIVE"
        reason = "Point estimates met the thresholds, but the 95% interval for run-level mean latency reduction did not establish a positive reduction. Collect more repetitions."
    else:
        decision = "ACCEPT"
        reason = "Adaptive policy met both performance thresholds and the run-level mean latency reduction interval was above zero for this tested workload."
    return {"decision": decision, "reason": reason, "criteria": criteria,
            "limits": {"scope": "tested synthetic foreground probe under background CPU contention", "not_a_general_performance_guarantee": True}}


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("results", type=Path, help="comparison_results.json")
    ap.add_argument("--policy-path", type=Path, required=True, help="selected_policy.json path")
    ap.add_argument("--evaluation-path", type=Path, required=True, help="policy_evaluation.json path")
    ap.add_argument("--min-latency-improvement", type=float, default=10.0)
    ap.add_argument("--min-background-retention", type=float, default=95.0)
    args = ap.parse_args()
    results = json.loads(args.results.read_text(encoding="utf-8"))
    decision = evaluate(results, args.min_latency_improvement, args.min_background_retention)
    decision["evaluated_at"] = datetime.now().astimezone().isoformat(timespec="seconds")
    decision["source_results"] = args.results.name
    args.evaluation_path.parent.mkdir(parents=True, exist_ok=True)
    args.evaluation_path.write_text(json.dumps(decision, indent=2), encoding="utf-8")
    if decision["decision"] == "ACCEPT":
        policy = dict(DEFAULT_POLICY)
        policy["formula"] = {**DEFAULT_POLICY["formula"], **results.get("policy_parameters_used", {})}
        policy["status"] = "accepted"
        policy["validation"] = {"status": "validated_for_tested_workload", "evaluated_at": decision["evaluated_at"],
                                "source_results": args.results.name, "criteria": decision["criteria"],
                                "scope": decision["limits"]["scope"]}
        args.policy_path.parent.mkdir(parents=True, exist_ok=True)
        args.policy_path.write_text(json.dumps(policy, indent=2), encoding="utf-8")
        decision["policy_action"] = "New selected_policy.json written; the benchmark can load these formula parameters on the next run."
    elif args.policy_path.exists():
        decision["policy_action"] = "Previous selected policy retained; rejected/inconclusive candidate was not promoted."
    else:
        decision["policy_action"] = "No policy promoted. The benchmark will use documented default formula parameters until an evaluation is accepted."
    args.evaluation_path.write_text(json.dumps(decision, indent=2), encoding="utf-8")
    print(f"Policy decision: {decision['decision']} — {decision['reason']}")
    print(f"Evaluation: {args.evaluation_path}")
    print(f"Selected policy: {args.policy_path if args.policy_path.exists() else 'none (defaults in use)'}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
