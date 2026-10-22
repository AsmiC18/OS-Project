# AegisOS Project Specification

## Title
Adaptive CPU Resource Allocation for Foreground Responsiveness Under Bursty Workloads

## Problem
Foreground applications need short response times while background jobs need CPU time to finish before their deadlines. Static allocation policies can favor foreground latency or background progress, but may behave differently as foreground demand changes.

## Research question
How does adaptive CPU allocation change the trade-off between foreground response latency and background jobs completed before their deadlines, compared with static policies, and under which workloads does it help or fail?

This is an evaluation question. The project does not claim to invent adaptive scheduling or to replace the Linux scheduler.

## Implementation
The project has two distinct components:

1. **Live Linux monitor/controller:** a Python user-space tool continuously samples real `/proc` process state and writes timestamped CSV snapshots and JSONL lifecycle events. It can be installed as a systemd user service for continuous monitoring. An opt-in control mode launches two explicitly selected commands in separate systemd user scopes and changes their cgroup v2 `CPUWeight` properties based on sampled CPU activity.
2. **C11 simulator:** a discrete-time model of one CPU in 1 ms ticks. It generates foreground requests and background jobs, tracks queues and CPU execution, applies five allocation policies, and records response-time and job-deadline metrics for repeatable experiments.

The live controller's heuristic and Linux `CPUWeight` mechanism are not identical to the simulator's discrete-time policies. Live mode currently measures CPU usage and memory residency; it does not directly measure UI input latency or background job deadlines.

## Policies
- Equal share: 50% foreground share while both queues are active.
- Fixed high share: 80% foreground share while both queues are active.
- Foreground priority: foreground work runs first whenever queued; latency-oriented reference.
- Background cap: background receives at most 50 CPU ticks per 100 ms window and cannot borrow unused quota.
- Adaptive: foreground share starts at 60%, increases by 10 percentage points when recent p95 latency exceeds 8 ms or the foreground queue grows, and decreases by 5 points when p95 is below 65% of target and the foreground queue is empty. Share is bounded between 35% and 90%.

## Metrics
- Foreground mean and p95 response latency.
- Foreground deadline-miss rate (latency above the 8 ms target).
- Background jobs completed and completed before deadline.
- Background deadline-miss rate.
- CPU utilization and background CPU execution.
- Adaptive controller changes and final foreground share.

## Experiment protocol
The default matrix runs five policies × four scenarios × 20 repetitions, using matched seeds across policies. `steady` and `sustained` are calibration scenarios. The analysis chooses the static policy with the greatest mean on-time background completions among those whose calibration mean p95 latency is at most 8 ms. `bursty` and `mixed` are held-out scenarios. The report compares adaptive and selected static policy on held-out seeds using paired bootstrap confidence intervals.

## Limitations
The simulator is a single-core, discrete-time model, not a real Linux benchmark. It omits kernel overhead, hardware/cache effects, I/O, multicore scheduling, and actual cgroup interfaces. The background cap is deliberately non-work-conserving and is not equivalent to Linux `cpu.weight`. Its results must be described as simulation results. The live controller's `CPUWeight` is a relative cgroup scheduling weight, not a fixed percentage cap; the controller affects only commands it explicitly launches, not arbitrary existing processes. Live CPU/RSS samples are real observations, but they do not by themselves establish application responsiveness or deadline success. Live control requires cgroup v2 and a working systemd user manager.
