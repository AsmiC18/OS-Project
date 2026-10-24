# Formula-based live CPU-weight control

## Purpose
AegisOS uses Linux systemd/cgroup-v2 `CPUWeight` values as relative scheduling weights. They are not CPU percentages or hard limits. The adaptive weights are calculated from live measurements rather than selected from a fixed table.

## Live benchmark formula
The benchmark measures the wall-clock latency of a bounded foreground CPU probe while a background CPU worker competes for CPU. Let:

- `L` = latest measured foreground probe latency in milliseconds;
- `T` = target latency in milliseconds (`--latency-threshold-ms`, default 8 ms);
- `r = max(0, L/T)` = normalized latency ratio;
- `S_f = clamp(0.50 + 0.20*r, 0.55, 0.90)` = calculated foreground share;
- total relative-weight budget `W = 1000`.

Then `foreground_weight = round_to_nearest_10(W*S_f)`, clamped to `[100, 900]`, and `background_weight = W - foreground_weight`. The pair therefore sums to 1000 and both groups retain at least 100 weight. Three consecutive samples above or below the target are required before the controller recalculates weights, reducing reactions to a single noisy sample.

Examples:

| Measured latency | Target | Ratio | Foreground share | Calculated weights |
|---:|---:|---:|---:|---:|
| 4 ms | 8 ms | 0.50 | 60% | 600 / 400 |
| 8 ms | 8 ms | 1.00 | 70% | 700 / 300 |
| 12 ms | 8 ms | 1.50 | 80% | 800 / 200 |
| 20 ms | 8 ms | 2.50 | 90% (cap) | 900 / 100 |

These are outputs of the formula, not a list the controller selects from. The target latency, response coefficient, share bounds, and total budget are policy parameters and should be justified/validated experimentally. The legacy benchmark mode preserves the original fixed 600/100 reference, but it is not treated as justified or optimal. The evaluation script now tests multiple fixed candidates in preliminary runs; adaptive mode starts at 500/500 and then calculates weights from measurements.

## Main live controller formula
The interactive live controller does not run the latency probe. It uses the CPU demand measured from `/proc` for the two managed systemd scopes:

`r_f = foreground_cpu / (foreground_cpu + background_cpu)` (zero if total measured demand is effectively zero)

`S_f = clamp(0.55 + 0.35*r_f, 0.55, 0.90)`

`foreground_weight = round_to_nearest_10(1000*S_f)` and `background_weight = 1000 - foreground_weight`.

This is a separate CPU-demand-based policy. Do not claim it is the same policy as the latency-based benchmark. Both formulas and their inputs are logged so a run can be audited.

## Baseline selection and use of evaluation results

A single inherited pair such as 600/100 is not enough to establish that adaptation is better. The comparison script therefore tests four fixed candidates in preliminary runs:

- Equal weights: 500/500 (throughput reference)
- Moderate foreground preference: 600/400
- Strong foreground preference: 800/200
- Legacy original-project reference: 600/100 (included for continuity, not assumed optimal)

For each candidate, it measures p95 foreground probe latency and background chunks completed. A candidate is eligible only if its mean background throughput retains at least the configured percentage of the equal-weight reference (default 95%). The script selects the eligible candidate with the lowest p95 latency. If none qualifies, it falls back to equal weights and flags that no candidate met the requirement.

The selected fixed pair is saved in `selected_fixed_baseline.json`. Crucially, the adaptive-versus-fixed final comparison uses fresh repetitions, separate from the preliminary runs used to choose the fixed baseline. This reduces the risk of reporting the same data both to select and validate a baseline.

The evaluator then accepts, rejects, or marks the adaptive policy inconclusive relative to that selected fixed baseline. Default criteria are at least 10% p95 latency reduction, at least 95% background-throughput retention relative to the selected fixed baseline, and a positive lower bound for the approximate 95% confidence interval of run-level mean latency reduction. These are engineering starting criteria, not universal truths; justify or revise them for the intended workload.

Only an `ACCEPT` promotes formula parameters to `selected_policy.json`. `REJECT` and `INCONCLUSIVE` do not overwrite a previously accepted policy. On a subsequent adaptive benchmark, the accepted formula parameters are loaded. A rejected candidate is not automatically tuned: adjust parameters deliberately, then evaluate with new independent runs. The legacy 600/100 pair is reported for continuity, but is not presumed optimal.

Run with `python3 scripts/aegisos_compare_live.py --duration 30 --repeats 3`. This performs 4 fixed candidate policies × repetitions, then selected fixed baseline × repetitions, then adaptive × repetitions. For quicker smoke tests use at least 10 seconds per run and 2 repetitions. Results are workload-specific and should not be described as universal performance guarantees. The selected latency formula applies to the benchmark controller; the separate interactive `/proc` CPU-demand controller has its own formula and is not automatically switched by this evaluation.

## Limitations
CPUWeight is relative to competing cgroups and does not guarantee a fixed CPU percentage. The benchmark is a controlled synthetic workload; it does not directly measure an interactive desktop application's latency. The formula parameters are explicit design choices, not mathematically optimal constants. They must be validated using the live comparison results.


## Evaluation-to-policy workflow (implemented)
The live comparison now invokes `scripts/aegisos_evaluate_policy.py` after writing `comparison_results.json`. It writes `policy_evaluation.json` with `ACCEPT`, `REJECT`, or `INCONCLUSIVE`, the metrics used, the configured thresholds, and the action taken. Default acceptance thresholds are at least 10% p95 latency reduction, at least 95% background-work retention, and a positive lower bound for the 95% confidence interval of run-level mean latency reduction. These are engineering starting criteria, not universal truths; change them with CLI options and justify them for the intended workload.

Only an `ACCEPT` writes/promotes `selected_policy.json`. `REJECT` and `INCONCLUSIVE` do not overwrite a previously accepted policy. On the next adaptive benchmark, the accepted policy formula parameters are loaded from `selected_policy.json`; the comparison logs which policy was loaded. If no policy has been accepted yet, documented defaults are used. The comparison report includes the decision and explains the action. A rejected candidate is not automatically tuned: adjust the documented formula parameters deliberately, then run a new independent comparison. Do not treat the same measurements used to tune a formula as independent validation.

Run with `python3 scripts/aegisos_compare_live.py --duration 30 --repeats 3`. For a stricter responsiveness threshold use `--min-latency-improvement 10`; for background retention use `--min-background-retention 95`. Results are workload-specific and should not be described as universal performance guarantees. The selected policy file applies to the latency-based benchmark controller; the separate interactive `/proc` CPU-demand controller has its own formula and is not automatically switched by this evaluation.
