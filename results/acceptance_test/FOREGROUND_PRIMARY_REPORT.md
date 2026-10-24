# AegisOS Foreground-Primary Acceptance Report

## Objective and acceptance rule

The primary objective for this test is to improve foreground P95 latency.
The candidate is accepted for this objective only if the paired-bootstrap
95% confidence interval for candidate-minus-baseline P95 latency is entirely
below zero. Background performance is reported as a secondary metric and
is not claimed to be unchanged.

This rule applies to the foreground-primary objective only. It does not
mean that both metrics improved, and it is not evidence of live-system
performance.

## Experimental evidence

- Source: `results/acceptance_test/all_policies.csv`
- Scenario: `bursty`
- Baseline: `fixed_high_share`
- Candidate: `foreground_priority`
- Matched seeds: 30 (50001–50030)
- Duration: 10,000 ms per run
- Comparisons: paired by seed
- Confidence intervals: 10,000 paired-bootstrap resamples

| Metric | Baseline mean | Candidate mean | Candidate minus baseline |
|---|---:|---:|---:|
| Foreground P95 latency (ms) | 7.000 | 5.000 | -2.000 |
| Background deadline-miss rate | 0.650210 | 0.650878 | 0.000667 |

- Foreground P95 difference 95% CI: [-2.000, -2.000] ms.
- Background miss-rate difference 95% CI: [0.000400, 0.000934].
- Background miss-rate change in percentage points: 0.0667.

## Decision

**ACCEPT — foreground-latency objective**

The measured foreground P95 latency difference is -2.000 ms.
The background deadline-miss rate changes by 0.000667.
This background trade-off must be disclosed when presenting the result.

Note: the P95 differences in the existing dataset are identical across
seeds. This produces a zero-width bootstrap interval; it may reflect the
simulator's deterministic behavior and must not be presented as proof of
real-world certainty. This acceptance is limited to the simulated workload
and the stated primary objective.

## Research justification

1. Asyabi, E. et al. (2019). "CTS: An operating system CPU scheduler to
   mitigate tail latency for latency-sensitive multi-threaded applications."
   Journal of Parallel and Distributed Computing, 133, 232–243.
   https://doi.org/10.1016/j.jpdc.2018.04.003

   Relevance: research on CPU scheduling and tail-latency reduction.
   It motivates measuring high-percentile foreground latency; it does not
   validate AegisOS's measured result.

2. Lo, D. et al. (2015). "Heracles: Improving Resource Efficiency at Scale."
   Proceedings of ISCA 2015.
   https://doi.org/10.1145/2749469.2749475

   Relevance: feedback-based resource management for latency-sensitive
   and best-effort workloads. It motivates reporting the foreground and
   background trade-off; its system and results differ from AegisOS.

3. Kalibera, T. and Jones, R. (2013). "Rigorous Benchmarking in Reasonable
   Time." Proceedings of ISMM 2013.
   https://doi.org/10.1145/2464157.2464160

   Relevance: repeated measurements and statistical treatment of
   experimental uncertainty. It motivates repeated matched runs and
   confidence intervals; it does not prescribe AegisOS's acceptance rule.

## Limitations

These results come from the AegisOS simulator and its predefined bursty
workload. They do not establish performance on a real Linux workload.
The acceptance criterion is a project-level choice, not a threshold
mandated by the cited papers. Background regression remains part of the
reported result.
