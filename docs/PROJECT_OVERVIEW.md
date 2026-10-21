# Project Overview

> Status: planning document. Nothing described as a feature here is implemented yet.

## Problem statement
Users running long-lived background applications on Linux may experience unexpected process
termination, sustained resource consumption, and repeated failures. Diagnosing these and
restarting failed workloads can require manual intervention.

## Objectives
1. Monitor registered applications through `/proc` with timestamped, reliable measurements.
2. Supervise application lifecycles (launch, wait, detect exit, restart with backoff and limits).
3. Detect anomalies with explainable, testable rules that ignore short spikes.
4. Separate detection from recovery; recover only under explicit, auditable policy.
5. Record structured incidents and export CSV; compute recovery statistics.
6. Provide a local web dashboard backed by a real C API.
7. Evaluate: detection delay, restart success rate, recovery time, false alerts, monitoring overhead.

## Scope
**In scope:** C on Linux; `/proc`, fork/exec/waitpid, signals; HTTP/JSON API in C; HTML/CSS/vanilla JS; Makefile.
**Out of scope (initially):** Docker, Node/React, database servers, cloud services, managing arbitrary
system processes, remote access. **Extension:** cgroups v2 CPU/memory budgets.

## Safety principles
- Act only on applications registered with or launched by AegisOS.
- API binds to 127.0.0.1, validates input, never executes arbitrary shell commands.
- Applications are launched via `execv`-style argument vectors from policy files, not through a shell.
- Dry-run and alert-only are the safe defaults.

## OS concepts demonstrated
Process management, `/proc` and system interfaces, signals, `fork`/`exec`/`wait`, process monitoring,
resource accounting, IPC/concurrency (API thread vs. supervisor loop), performance analysis,
(extension) cgroups.

## Academic mapping (40 marks)
Proposal 10 · Design & progress report 10 · Final report 10 · Final presentation 10.
Guidelines also require implementation, testing, performance evaluation, error handling,
documentation, a user manual, and a demonstration.
> Note: the guidelines PDF says Review I expects "at least 60–70%" implementation, while the
> VTOP table says progress report "40–50%". Confirm the target with the course instructor.
