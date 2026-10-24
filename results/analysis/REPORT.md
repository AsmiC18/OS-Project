# AegisOS simulation results

Input: `results/raw_results.csv`. Runs analyzed: 400.

These are results from the bundled simplified simulator, not measurements from a real Linux system. Confidence intervals are percentile bootstrap intervals for run-level p95 latency. They summarize variation across seeds; they do not correct for model limitations.

**Predefined foreground p95 target:** 8.0 ms. **Calibration scenarios:** steady, sustained. **Held-out scenarios:** bursty, mixed.

**Static policy selected using calibration scenarios only:** `fixed_high_share` (mean p95 5.00 ms; mean on-time background completions 280.57). Selection rule: among static policies meeting the calibration p95 target, maximize on-time background completions.

## Summary

| Scenario | Policy | Runs | Mean p95 (ms) | Median p95 (ms) | Bootstrap 95% CI for mean p95 | BG jobs on time | BG deadline-miss rate | Mean CPU utilization | Controller changes |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| bursty | adaptive | 20 | 6.0500 | 6.0000 | [6.0000, 6.1500] | 334.50 | 0.66532 | 0.98237 | 2.95 |
| bursty | background_cap | 20 | 17.0500 | 18.0000 | [15.8500, 18.2500] | 3.65 | 0.99634 | 0.63170 | 0.00 |
| bursty | equal | 20 | 18.7000 | 18.0000 | [17.3000, 20.1000] | 341.30 | 0.65852 | 0.98237 | 0.00 |
| bursty | fixed_high_share | 20 | 6.9500 | 7.0000 | [6.8500, 7.0000] | 334.75 | 0.66507 | 0.98237 | 0.00 |
| bursty | foreground_priority | 20 | 5.0000 | 5.0000 | [5.0000, 5.0000] | 334.05 | 0.66577 | 0.98237 | 0.00 |
| mixed | adaptive | 20 | 14.3000 | 7.0000 | [10.0500, 19.1500] | 740.80 | 0.11068 | 0.81471 | 12.35 |
| mixed | background_cap | 20 | 15.2000 | 15.0000 | [14.2000, 16.3000] | 5.40 | 0.99349 | 0.60840 | 0.00 |
| mixed | equal | 20 | 15.7000 | 16.0000 | [14.7000, 16.8000] | 744.60 | 0.10612 | 0.81471 | 0.00 |
| mixed | fixed_high_share | 20 | 6.6500 | 7.0000 | [6.4500, 6.8500] | 738.65 | 0.11327 | 0.81471 | 0.00 |
| mixed | foreground_priority | 20 | 5.0000 | 5.0000 | [5.0000, 5.0000] | 737.95 | 0.11411 | 0.81471 | 0.00 |
| steady | adaptive | 20 | 7.0500 | 7.0000 | [7.0000, 7.1500] | 555.30 | 0.00000 | 0.63969 | 0.05 |
| steady | background_cap | 20 | 8.0000 | 8.0000 | [8.0000, 8.0000] | 555.10 | 0.00000 | 0.63948 | 0.00 |
| steady | equal | 20 | 8.0000 | 8.0000 | [8.0000, 8.0000] | 555.30 | 0.00000 | 0.63969 | 0.00 |
| steady | fixed_high_share | 20 | 5.0000 | 5.0000 | [5.0000, 5.0000] | 555.30 | 0.00000 | 0.63969 | 0.00 |
| steady | foreground_priority | 20 | 4.0000 | 4.0000 | [4.0000, 4.0000] | 555.30 | 0.00000 | 0.63969 | 0.00 |
| sustained | adaptive | 20 | 7.0000 | 7.0000 | [7.0000, 7.0000] | 5.95 | 0.99403 | 1.00000 | 0.00 |
| sustained | background_cap | 20 | 9.1000 | 9.0000 | [9.0000, 9.2500] | 5.25 | 0.99473 | 0.92868 | 0.00 |
| sustained | equal | 20 | 10.0500 | 10.0000 | [10.0000, 10.1500] | 6.10 | 0.99388 | 1.00000 | 0.00 |
| sustained | fixed_high_share | 20 | 5.0000 | 5.0000 | [5.0000, 5.0000] | 5.85 | 0.99413 | 1.00000 | 0.00 |
| sustained | foreground_priority | 20 | 4.0000 | 4.0000 | [4.0000, 4.0000] | 5.70 | 0.99428 | 1.00000 | 0.00 |

## Adaptive policy versus calibrated static policy on held-out scenarios

Positive p95 difference means adaptive latency is worse; positive background-completion difference means adaptive completes more background jobs on time. Intervals are paired bootstrap CIs across matched seeds.

| Scenario | p95 difference (adaptive - static), ms | Paired bootstrap 95% CI | On-time BG completion difference | Paired bootstrap 95% CI |
|---|---:|---:|---:|---:|
| bursty | -0.90 | [-1.00, -0.75] | -0.25 | [-0.50, 0.00] |
| mixed | 7.65 | [3.20, 12.70] | 2.15 | [0.60, 3.95] |

## Held-out scenario decisions

Differences are adaptive minus static, paired by seed. Negative P95 and background deadline-miss-rate differences favor adaptive scheduling. Both metrics are reported; no arbitrary acceptable-loss threshold is assumed.

| Scenario | P95 difference (ms) | P95 95% CI | Background deadline-miss-rate difference | Background 95% CI | Result |
|---|---:|---:|---:|---:|---|
| bursty | -0.90 | [-1.00, -0.75] | 0.00025 | [0.00000, 0.00050] | INCONCLUSIVE: mixed trade-off or uncertainty |
| mixed | 7.65 | [3.20, 12.70] | -0.00258 | [-0.00474, -0.00072] | INCONCLUSIVE: mixed trade-off or uncertainty |

## Interpretation cautions

- Lower foreground p95 and foreground deadline-miss rate are better. Higher on-time background completions are better. Consider both metrics together.
- `background_cap` is an intentionally strict, non-borrowing cap; it is not equivalent to Linux `cpu.weight`.
- Static policy selection uses only the designated calibration scenarios. Bursty and mixed scenarios are held out for the final comparison.
- Do not claim adaptive superiority unless the held-out measurements support it. This simulator models one CPU in 1 ms steps and omits hardware, cache, I/O, kernel, and multicore effects.
