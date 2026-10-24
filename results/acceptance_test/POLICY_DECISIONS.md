# Policy comparison

Differences are candidate minus fixed_high_share. Negative values favor the candidate.

| Scenario | Policy | P95 difference (ms) | P95 95% CI | Background miss-rate difference | Miss-rate 95% CI | Decision |
|---|---|---:|---|---:|---|---|
| bursty | equal | 11.267 | [10.333, 12.200] | -0.00680 | [-0.00777, -0.00587] | INCONCLUSIVE |
| bursty | foreground_priority | -2.000 | [-2.000, -2.000] | 0.00067 | [0.00040, 0.00093] | INCONCLUSIVE |
| bursty | background_cap | 10.000 | [9.067, 10.933] | 0.34611 | [0.32560, 0.36608] | REJECT |
| bursty | adaptive | -0.433 | [-0.700, -0.133] | -0.00023 | [-0.00063, 0.00013] | INCONCLUSIVE |
| mixed | equal | 9.600 | [8.700, 10.567] | -0.00728 | [-0.00836, -0.00620] | INCONCLUSIVE |
| mixed | foreground_priority | -1.467 | [-1.633, -1.300] | 0.00156 | [0.00100, 0.00216] | INCONCLUSIVE |
| mixed | background_cap | 8.733 | [7.933, 9.600] | 0.87946 | [0.87295, 0.88547] | REJECT |
| mixed | adaptive | 6.967 | [3.400, 10.833] | -0.00300 | [-0.00480, -0.00148] | INCONCLUSIVE |
| steady | equal | 3.000 | [3.000, 3.000] | 0.00000 | [0.00000, 0.00000] | REJECT |
| steady | foreground_priority | -1.000 | [-1.000, -1.000] | 0.00000 | [0.00000, 0.00000] | INCONCLUSIVE |
| steady | background_cap | 3.000 | [3.000, 3.000] | 0.00006 | [0.00000, 0.00018] | REJECT |
| steady | adaptive | 2.133 | [2.033, 2.267] | 0.00000 | [0.00000, 0.00000] | REJECT |
| sustained | equal | 5.067 | [5.000, 5.167] | -0.00044 | [-0.00064, -0.00023] | INCONCLUSIVE |
| sustained | foreground_priority | -1.000 | [-1.000, -1.000] | 0.00007 | [0.00000, 0.00017] | INCONCLUSIVE |
| sustained | background_cap | 4.067 | [4.000, 4.167] | 0.00060 | [0.00017, 0.00107] | REJECT |
| sustained | adaptive | 2.000 | [2.000, 2.000] | -0.00020 | [-0.00033, -0.00007] | INCONCLUSIVE |
