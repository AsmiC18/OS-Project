# AegisOS Live Monitoring and Control Design

## Components

- `scripts/aegisos_live.py monitor`: periodically reads visible `/proc/<pid>/stat`, `cmdline`, `cgroup`, and memory resident-set size. It writes timestamped process samples to CSV and lifecycle/session/controller events to JSONL.
- `scripts/aegisos_live.py doctor`: reports whether `/proc`, cgroup v2 CPU control, and the `systemd --user` manager appear available. A passing check is a prerequisite check, not proof that every scope property update will succeed.
- `scripts/aegisos_live.py run`: launches only two explicitly supplied commands in separate user-systemd scopes. It samples process CPU time and applies bounded relative `CPUWeight` choices to those scopes.
- `scripts/aegisos_dashboard.py`: local standard-library HTTP server; provides live process snapshots, event data, an HTML dashboard, HTML-formatted report page, and report/summary downloads. Default bind is `127.0.0.1`.
- `scripts/summarize_live.py`: derives a Markdown report and CSV summary from recorded logs.
- `src/aegisos_sim.c`: independent deterministic simulator for five CPU allocation policies and four synthetic workload scenarios.

## Live controller rule

The live controller sums sampled CPU percentage for processes found in each managed scope:

- If foreground-scope CPU is at least 2%, request foreground/background weights `800/100`.
- Else if background-scope CPU is at least 2%, request weights `100/300`.
- Else request `300/100`.

The controller updates weights only when the requested pair differs from its last successfully applied pair. It records successful changes as `cpu_weight_changed`, failed updates as `cpu_weight_update_failed`, and attempts a best-effort rollback if the second property update fails.

These weights are relative CPU scheduling weights among competing cgroups, not a percentage allocation, CPU quota, or latency guarantee. The heuristic measures CPU activity, not keyboard/mouse latency or job deadlines. It controls only the commands launched by this command invocation.

## Safety boundaries

- Monitoring mode is read-only with respect to scheduling.
- Controller mode is opt-in and starts the two exact commands provided by the user. It stops those managed scopes when the controller exits.
- Do not use controller mode for critical system services or commands with unsaved work.
- The dashboard binds to localhost by default and has no authentication; do not expose it to an untrusted network.
- WSL may provide `/proc` while lacking a working user systemd manager. In that case, monitoring and reporting work but live CPUWeight control does not.

## Data and reports

Each run creates a new `processes-<timestamp>.csv` and `events-<timestamp>.jsonl` pair. Files are append-only within that session and remain after the monitor stops. The dashboard process table shows the most recent snapshot from the newest session; the history chart and report summarize recorded rows across available sessions. `LIVE_REPORT.md` and `live_process_summary.csv` are derived outputs and can be regenerated at any time.

The simulator outputs under `results/` are not live logs. Keep the two evidence sources separate in analysis and presentations.
