# Implementation Plan

> Status key: ☐ not started. At scaffold time **all phases are ☐** except the scaffold itself.
> No dates or completion percentages are claimed; the team should add them.

The order makes the backend useful (CLI + logs) before the dashboard exists.

## Phase 1: Baseline and build setup — ◐ partly done
Done in scaffold: original ZIP inspected (`docs/LEGACY_ANALYSIS.md`), legacy preserved, `make` builds a stub.
Remaining: confirm toolchain on every teammate's machine (Linux/WSL2), start Git history, agree coding style.
**Exit:** everyone can `make && make test` and debug in VS Code.

## Phase 2: Monitoring ☐
`process` readers, `monitor` sampling, CPU% and RSS, vanished-PID handling, timestamps, `test_monitor`.
**Exit:** correct CPU%/RSS for a known busy workload, verified against `top`/`ps` as a sanity check.

## Phase 3: Process supervision ☐
Launch via argv (no shell), `waitpid`, exit status, restart with exponential backoff and max retries, graceful shutdown, cleanup.
**Exit:** `crash_worker` is restarted per policy and stops at the limit; no zombies/leaks.

## Phase 4: Anomaly detection ☐
Thresholds, sustained windows, severity, memory/crash/restart-loop rules, explanation text.
**Exit:** short spike → no alert; sustained load → alert with recorded rule and values.

## Phase 5: Recovery engine ☐
Policy parsing, alert-only/dry-run, authorized restart, cooldown, verification, audit log.
**Exit:** actions impossible on non-registered PIDs; dry-run performs no action.

## Phase 6: Persistence and reporting ☐
Incident history, filtering, CSV export, summary statistics. Decide JSONL vs. SQLite.

## Phase 7: C HTTP API ☐
Local JSON API (`docs/API_SPECIFICATION.md`), validation, loopback binding, error handling, `test_api.sh`.

## Phase 8: Dashboard ☐
Wire the seven sections to real API responses; authorized controls; no hardcoded metrics.

## Phase 9: Testing and performance evaluation ☐
Repeatable workloads, regression and failure tests, benchmark scripts for detection delay, restart success rate,
recovery time, false alerts, monitoring overhead. Record real results only.

## Phase 10: Finalization ☐
Documentation, user manual, screenshots, final report, presentation (15–20 slides), demo rehearsal.

## Optional extension ☐
cgroups v2 CPU/memory budgets for controlled workloads.

## Review alignment
Review I needs a core, executable prototype (see the 40–50% vs. 60–70% note in `PROJECT_OVERVIEW.md`):
target Phases 1–4 (ideally 5) before Review I. Review II: all phases, testing, evaluation, viva.
