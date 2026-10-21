# Architecture

> Status: intended architecture. Only the folder/module layout exists in code.

## Overview
```
 Browser dashboard (HTML/CSS/JS)
          │  HTTP/JSON, 127.0.0.1 only
   ┌──────▼───────┐
   │  api_server  │  fixed routes, input validation
   └──────┬───────┘
          │ reads state / submits approved actions
 ┌────────▼───────────────────────────────────────────┐
 │ core loop                                          │
 │  monitor ──samples──► anomaly ──events──► recovery │
 │     ▲                     ▲                  │      │
 │  process (/proc)       policy ◄──────────────┤      │
 │                                              ▼      │
 │  supervisor (fork/exec/waitpid, backoff) ◄───┘      │
 └────────┬───────────────────────────────────────────┘
          ▼
   logger ──► data/ logs/  ──► reports (query, CSV, stats) ──► reports/
```

## Data flow
1. `supervisor` launches registered apps and owns their PIDs and exit statuses.
2. `monitor` samples each watched PID every interval via `process` readers (`/proc/<pid>/stat`, `statm`, `status`).
3. `anomaly` evaluates rules (e.g. CPU above threshold for N seconds) and emits explained events.
4. `recovery` consults `policy` to choose an action (alert, dry-run, restart, await approval), enforces
   cooldown/limits, executes through `supervisor`, and verifies the outcome.
5. `logger` records every sample-derived event, decision and outcome; `reports` queries and exports.
6. `api_server` exposes read-only state plus a small set of authorized actions.

## Concurrency model (to be confirmed in Phase 3/7)
Preferred: one core loop (poll with timeout, `waitpid(WNOHANG)` / SIGCHLD flag) plus one API thread (pthread)
sharing state under a mutex. Alternative if simpler: a single-threaded `poll()` loop serving both.

## Technology
C (gnu11), Linux, `/proc`, POSIX process APIs, Makefile, VS Code/gdb, HTML/CSS/vanilla JS, CSV; SQLite optional.

## Persistence (decision deferred to Phase 6)
Start with append-only JSON Lines for events and CSV export. Adopt SQLite only if query/filter needs justify the dependency.

## Extension: resource budgets
cgroups v2 (`cpu.max`, `memory.max`) for controlled workloads. Availability and delegation vary (notably on WSL2);
verify before planning around it. Not required for the initial version.

## Retained / replaced / dropped (from the baseline)
See `docs/LEGACY_ANALYSIS.md`.
