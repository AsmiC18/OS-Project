# AegisOS Separate Acceptance Experiment

- Runs: 240 total (30 matched seeds × 4 scenarios × 2 policies).
- Comparison: adaptive minus fixed_high_share.
- Negative P95 difference favors adaptive.
- Negative background deadline-miss-rate difference favors adaptive.
- Confidence intervals use paired bootstrap resampling (95%).

| Scenario | P95 difference (ms) | P95 95% CI | Background miss-rate difference | Miss-rate 95% CI | Decision |
|---|---:|---|---:|---|---|
| bursty | -0.433 | [-0.733, -0.133] | -0.00023 | [-0.00063, 0.00013] | INCONCLUSIVE: trade-off or uncertainty |
| mixed | 6.967 | [3.433, 10.900] | -0.00300 | [-0.00476, -0.00148] | INCONCLUSIVE: trade-off or uncertainty |
| steady | 2.133 | [2.033, 2.267] | 0.00000 | [0.00000, 0.00000] | REJECT: P95 worsened without miss-rate benefit |
| sustained | 2.000 | [2.000, 2.000] | -0.00020 | [-0.00037, -0.00007] | INCONCLUSIVE: trade-off or uncertainty |

## Interpretation

A scenario is accepted only when both confidence intervals favor adaptive scheduling. Inconclusive results are not counted as accepted. This is a separate simulation experiment, not evidence of live-system performance.
