# AegisOS: Live Linux Process Monitor and Adaptive CPU Controller

AegisOS now has two parts: (1) a live Ubuntu process monitor/controller that reads real `/proc` data, continuously records processes and events, and can adjust cgroup v2 CPU scheduling weights for two explicitly launched commands; and (2) a C simulator for repeatable policy experiments. The simulator results already in `results/` are simulated and must not be described as live measurements.

**Live mode is opt-in.** `monitor` mode only observes and logs. `run` mode launches the two commands you specify in separate `systemd --user` scopes and changes their `CPUWeight` values based on sampled CPU activity. It does not move or modify arbitrary existing processes. CPU weights are relative scheduling weights, not fixed CPU percentages or hard quotas. Live control requires Ubuntu with cgroup v2 and a working user systemd manager; if those requirements are missing, the controller exits with an error instead of pretending it is controlling CPU allocation.

## Research question

How does adaptive CPU allocation change the trade-off between foreground response latency and background jobs completed before their deadlines, compared with static policies, and under which workloads does it help or fail?

## Policies

- `equal`: approximately equal CPU share while foreground and background work compete.
- `fixed_high_share`: fixed 80% foreground share during contention.
- `foreground_priority`: run foreground work first whenever it is queued; a latency-oriented reference policy.
- `background_cap`: limit background CPU execution to 50 ms per 100 ms window. It uses equal sharing during contention until the cap is reached and cannot borrow unused quota when foreground work is absent. This is a strict cap, not Linux's work-conserving `cpu.weight` behavior.
- `adaptive`: adjust foreground share between configured bounds based on recent foreground p95 latency and queue depth.

## Workload scenarios

`steady`, `bursty`, `sustained`, and `mixed`. Each simulation is deterministic for a given scenario and seed. The simulator uses 1 ms time steps and a single modeled CPU. Foreground requests and finite background jobs arrive according to scenario-specific patterns. Background jobs have deadlines, allowing the experiment to measure how many finish on time.

## Requirements

- GCC or Clang with C11 support
- GNU Make
- Python 3 (for result analysis)
- Optional: `matplotlib` for graphs

## Build and test

```bash
make
make test
./build/aegisos-sim --help
```

## Run one simulation

```bash
./build/aegisos-sim run --policy adaptive --scenario bursty --seed 7 --duration 10000
./build/aegisos-sim run --policy equal --scenario bursty --seed 7 --duration 10000 --csv results/one_run.csv
```

The output reports generated/completed foreground requests, mean and p95 latency, foreground deadline-miss rate, background CPU work, background jobs completed on time, background deadline misses, CPU utilization, controller changes, and the final foreground share. The background deadline-miss rate is calculated over jobs whose deadline outcome is known (on-time completions plus misses).

## Run AegisOS live on Ubuntu

### Fastest path: start monitor and HTML dashboard together

From the extracted project directory, run:

```bash
python3 scripts/aegisos_live.py doctor
bash scripts/start_aegisos.sh
```

The launcher starts the monitor in the background and serves the local dashboard in the foreground. Open `http://127.0.0.1:8765/`; use the **Live Report** link for the HTML report. Press `Ctrl+C` in the launcher terminal to stop both components. Saved logs remain under `live_data/`. The dashboard binds to localhost only.

To start the components separately instead, use the commands in sections A and C below.

Install the live-mode prerequisites if needed:

```bash
sudo apt update
sudo apt install python3 systemd
```

**WSL note:** monitoring works in WSL because it reads `/proc`, but live CPU control needs a working `systemd --user` manager and cgroup v2. On WSL 2, systemd may need to be enabled in `/etc/wsl.conf`:

```ini
[boot]
systemd=true
```

After saving that file, run `wsl --shutdown` from Windows PowerShell, restart Ubuntu, then check `systemctl --user is-system-running` and `python3 scripts/aegisos_live.py doctor`. If the doctor reports the user systemd manager is unavailable, monitor and dashboard modes still work, but live CPU-weight control is not available until the environment is configured.

### A. Continuously monitor real processes (read-only)

```bash
python3 scripts/aegisos_live.py monitor --interval 1 --log-dir live_data
```

This runs continuously until you press `Ctrl+C`. It refreshes a terminal dashboard and continuously saves timestamped files under `live_data/`:

- `processes-*.csv`: sampled PID, PPID, command, state, CPU%, RSS, cgroup and timestamp.
- `events-*.jsonl`: process discovery/exit events and session metadata.

#### Keep monitoring running as a user service

To run the monitor in the background and have systemd restart it if it fails:

```bash
bash scripts/install_user_service.sh
systemctl --user status aegisos-monitor.service
```

The service runs continuously in monitor-only mode; it does not alter CPU scheduling. Stop it with `systemctl --user stop aegisos-monitor.service`. To disable startup, use `systemctl --user disable aegisos-monitor.service`. Logs remain in `live_data/`.

Stop monitoring, then generate a report from the saved real observations:

```bash
python3 scripts/summarize_live.py --log-dir live_data
```

This writes `live_data/LIVE_REPORT.md` and `live_data/live_process_summary.csv`. These files are separate from the simulator report in `results/`.

### B. Run the live adaptive CPU-weight controller

This mode launches two commands that you explicitly choose, assigns them to separate user-systemd cgroups, samples their real CPU use, and updates their cgroup `CPUWeight` properties while they run:

```bash
python3 scripts/aegisos_live.py run \
  --foreground "python3 scripts/cpu_burn.py --seconds 120" \
  --background "python3 scripts/cpu_burn.py --seconds 120" \
  --interval 1 --log-dir live_data
```

The included bounded CPU workload is for a controlled demonstration. For real workloads, replace the quoted commands with applications or jobs you intend AegisOS to manage. **The commands are launched by AegisOS and are stopped when its controller is stopped.** Do not pass critical system services or unsaved work.

Requirements and safety notes:

- Use a local Ubuntu session with a running `systemd --user` manager and cgroup v2 mounted at `/sys/fs/cgroup`.
- Run `python3 scripts/aegisos_live.py doctor` before attempting control. It checks `/proc`, cgroup v2, and the user systemd manager. Passing the check means prerequisites are present; actual scope property changes are only confirmed by `cpu_weight_changed` events in the logs.
- If systemd refuses `CPUWeight` changes, live control has not been applied; check the error output and session/cgroup configuration. Failed changes are logged as `cpu_weight_update_failed` and the controller attempts a best-effort rollback of partial changes.
- `CPUWeight` is a relative weight among competing cgroups. It is not a hard CPU cap and does not guarantee a particular percentage of CPU time.
- The controller's current heuristic boosts the foreground scope when it is actively consuming CPU and favors background progress when the foreground scope is mostly idle. It does not infer application input latency or guarantee foreground responsiveness.
- The monitor can observe only processes permitted by the current user's `/proc` visibility and permissions.
- `Ctrl+C` stops the managed scopes and monitoring loop. Logs already written remain on disk.

### C. Open the live browser dashboard

Keep the monitor command running in one terminal. Open a second terminal in the project directory and run:

```bash
python3 scripts/aegisos_dashboard.py --log-dir live_data
```

Then open **http://127.0.0.1:8765** in your browser. The dashboard refreshes every two seconds and shows sampled processes, CPU and memory readings, a recent CPU activity chart, and logged events. Use the **Live Report** link in the header to view an HTML-formatted report generated from your actual saved monitoring logs; that report refreshes every 10 seconds while open. **Download Report** saves `LIVE_REPORT.md`, and the report page also links to `live_process_summary.csv`. These reports are separate from the simulator report in `results/`. The server uses Python’s standard library; no extra packages are required. The dashboard is read-only and does not itself change CPU scheduling. Press `Ctrl+C` in the dashboard terminal to stop the web page; this does not stop the separate monitor process. The server binds to localhost by default and is not exposed to your network.

### D. Generate a live report

After collecting live data (monitoring can be stopped or still running):

```bash
python3 scripts/summarize_live.py --log-dir live_data
```

The live report includes sampled process CPU/RSS summaries and, in controller mode, recorded CPU-weight decisions. Do not merge live observations with simulator CSVs; they measure different things.

## Run the experiment matrix

```bash
make experiment
```

By default this runs 5 policies × 4 scenarios × 20 repetitions = 400 simulations, writes raw observations to `results/raw_results.csv`, and creates `results/summary.csv` and `results/REPORT.md`. If matplotlib is installed, it also creates plots in `results/`.

You can change the matrix size manually:

```bash
./build/aegisos-sim experiment --runs 5 --duration 5000 --out results/pilot.csv
python3 scripts/analyze_results.py results/pilot.csv --out results/pilot_analysis
```

## Interpretation and limitations

- This is a simplified single-core, discrete-time model. It does not model cache effects, I/O, kernel overhead, multicore scheduling, or real Linux cgroup behavior.
- `background_cap` is intentionally a strict, non-borrowing cap; it should not be described as equivalent to Linux `cpu.weight`.
- The adaptive policy is a hypothesis, not a guaranteed improvement. Static policies may perform as well as or better than it.
- The bundled analysis uses `steady` and `sustained` for static-policy selection and holds out `bursty` and `mixed` for comparison. This is a reproducible experiment, not proof of general superiority.
- Report measured results as simulated results, not as real-machine benchmarks.

## Repository layout

```text
include/aegisos_sim.h    Public simulator types and API
src/aegisos_sim.c        Workload model, scheduler, controller, metrics
src/main.c               CLI and experiment runner
tests/test_sim.c         Determinism, bounds, input validation tests
scripts/analyze_results.py
results/                 Generated CSV, report, and optional plots
archive/                 Original process-monitoring project retained for reference
```

## Experimental protocol

The analysis script treats `steady` and `sustained` as calibration scenarios and `bursty` and `mixed` as held-out scenarios. It selects one static policy using only calibration results: among static policies meeting the 8 ms mean-p95 target, it chooses the one with the highest mean number of background jobs completed before their deadlines. It then compares the adaptive policy with that selection on the held-out scenarios using paired seeds and paired bootstrap confidence intervals. This is a predefined project protocol, not evidence that the adaptive policy is guaranteed to win.

## Reproducibility

Record the source commit, compiler/version, command line, duration, seeds, and raw CSV when submitting results. Do not tune policy parameters on the same runs used for final evaluation. Keep failed runs and document exclusions.

## Current experiment results

The checked-in `results/raw_results.csv`, `results/summary.csv`, `results/REPORT.md`, and PNG plots were generated by running the implementation (5 policies × 4 scenarios × 20 repetitions). They are example simulation results, not fabricated or real-machine measurements. In the bundled run, adaptive control improves foreground p95 latency over equal sharing in the bursty scenario while on-time background completions are nearly unchanged; on the mixed held-out scenario, latency results are less stable and the adaptive policy has a worse mean p95 than the calibrated static policy. This mixed result is intentional evidence of a limitation, not a success claim.

## Full browser dashboard (updated)

The local browser dashboard now presents the project as a multi-section console:

- **Overview:** purpose, project components, current live metrics, and a concise interpretation.
- **Live Monitor:** latest process snapshot, sampled CPU history, memory totals, recent events, and process filtering.
- **CPU Controller:** recorded controller state, last successful CPUWeight change, and controller event timeline.
- **Simulation Lab:** policy comparisons by scenario using `results/summary.csv`, including foreground p95 latency, on-time background jobs, background deadline misses, utilization, and controller changes.
- **Implementation:** maps the dashboard to source files and explains the five simulator policies.
- **Reports:** links to the live report, simulation report, raw results, and summary CSV.

### Start the browser dashboard

From the project root in WSL:

```bash
python3 scripts/aegisos_dashboard.py --log-dir live_data
```

Open `http://127.0.0.1:8765/` in your Windows browser. Keep the terminal running. For fresh live process information, start monitoring in a second terminal:

```bash
python3 scripts/aegisos_live.py monitor --interval 1 --log-dir live_data
```

For a live CPU-controller demonstration, stop the monitor-only run and execute the documented `aegisos_live.py run` command with foreground and background workloads. The dashboard reads its event logs; it does not start or stop workloads and does not directly modify scheduling.

The dashboard loads simulator aggregates from `results/summary.csv` and `results/REPORT.md` automatically. If live logs do not exist yet, the simulation sections still work while live-monitor metrics show that no samples have been recorded. Browser controls can start and stop monitor sampling and the bounded controller demo. If you already started a monitor manually in another terminal, do not start a second monitor from the browser; stop the existing one first.

### Browser disconnect message

A `BrokenPipeError` in the dashboard server can occur when the browser navigates away or cancels an in-progress auto-refresh request. The dashboard now suppresses expected broken-pipe/connection-reset tracebacks. This does not indicate a CPU-controller failure; check the controller event log separately for `cpu_weight_update_failed` events.

## Live adaptive responsiveness benchmark

The simulation remains a repeatable model, but it is not the live performance proof. To measure real behavior on a supported Ubuntu/WSL system, run this in a terminal from the project root:

```bash
python3 scripts/aegisos_live_benchmark.py --log-dir live_data --duration 60
```

The benchmark launches a bounded foreground CPU probe and a separate background CPU load in dedicated user systemd scopes. It records each foreground probe's **wall-clock completion time** (which includes scheduling delay), then changes the real scope `CPUWeight` after consecutive samples cross the configured latency threshold. The dashboard reads the latest probe measurement and the benchmark's events from `live_data`.

Useful options:

- `--duration 60`: run for 60 seconds; omit or use `0` to continue until Ctrl+C.
- `--latency-threshold-ms 8`: measured probe latency threshold for controller decisions.
- `--work 250000`: amount of bounded foreground work per probe.
- `--probe-interval 0.2`: time between probes.

This requires cgroup v2, a working systemd user manager, and permission to set CPUWeight. Check the environment first with `python3 scripts/aegisos_live.py doctor`. In some WSL configurations systemd user scopes or CPU controller delegation may not be available; the benchmark will report that instead of claiming a live run succeeded. CPUWeight is a relative scheduling weight, not a CPU percentage cap. The benchmark is a controlled workload, not a claim that it measures the response time of an arbitrary desktop application.


## Live benchmark: real latency and adaptive CPU weights

Run these commands from the project root in Ubuntu/WSL2. First check support:

```bash
python3 scripts/aegisos_live.py doctor
```

Start the real benchmark (60 seconds):

```bash
python3 scripts/aegisos_live_benchmark.py --log-dir live_data --duration 60
```

In another terminal, serve the dashboard:

```bash
python3 scripts/aegisos_dashboard.py --log-dir live_data
```

Open `http://127.0.0.1:8765/`. The CPU Controller page now displays the latest measured probe latency, a latency history chart, and the latest recorded threshold/decision. The dashboard refreshes these values every two seconds. These measurements describe the controlled benchmark task, not arbitrary application latency. The simulation lab remains a separate, explicitly labelled experimental model.

## Live performance comparison

Start the dashboard from the project directory:

```bash
python3 scripts/aegisos_dashboard.py --log-dir live_data
```

Open `http://127.0.0.1:8765/`, select **Live Evaluation**, and choose **Run comparison**. The dashboard runs three 30-second repetitions for each mode, then displays mean, median, p95, and maximum foreground probe latency, background CPU-work chunks completed, percentage changes, and a 95% confidence interval for the mean latency reduction. Results and report are saved under `live_data/comparison/`.

The comparison measures a foreground CPU probe and counts completed fixed-size background CPU-work chunks under contention. It does not directly measure desktop-application response time. Repeated runs and confidence intervals help evaluate consistency.


### Live adaptive-policy evaluation
Run `python3 scripts/aegisos_compare_live.py --duration 30 --repeats 3` in a Linux login session where user systemd and cgroup v2 CPU control are available. The run produces `live_data/comparison/comparison_results.json`, `LIVE_COMPARISON_REPORT.md`, and `policy_evaluation.json`. An accepted result writes `selected_policy.json`; rejected or inconclusive results preserve any previously accepted policy. Default criteria are >=10% p95 latency reduction, >=95% background-work retention, and a positive lower bound for the run-level mean-latency reduction confidence interval. These thresholds are configurable and must be validated for the target workload.

## Live adaptive-policy evaluation and baseline selection

Run `python3 scripts/aegisos_compare_live.py --duration 30 --repeats 3` in a Linux environment with user systemd and cgroup v2 CPU control available. The comparison first benchmarks fixed-weight candidates (500/500, 600/400, 800/200, and the legacy 600/100 reference). It uses equal weights as a background-throughput reference, selects the eligible fixed policy with the lowest p95 probe latency, then compares the adaptive controller against that policy in fresh runs. See `docs/FORMULA_BASED_LIVE_CONTROL.md` for the formula, acceptance criteria, output files, and limitations. The legacy 600/100 pair is not assumed optimal.

## Connected live-evaluation dashboard

The browser dashboard is connected to the formula-based live evaluation and baseline-selection workflow. Start it from the project root:

```bash
python3 scripts/aegisos_dashboard.py --log-dir live_data
```

Open `http://127.0.0.1:8765/` and select **Live Evaluation**. The dashboard lets you choose seconds per run (10–300) and repetitions (2–10), start or stop the full evaluation, and view:

- progress through preliminary fixed-policy candidate runs and final fresh-run comparison;
- candidate p95 latency and background-throughput retention relative to equal weights;
- the selected fixed baseline and the reason it was selected;
- mean, p50, p95, and maximum foreground probe latency plus background CPU-work chunks;
- the `ACCEPT`, `REJECT`, or `INCONCLUSIVE` policy decision, including the acceptance criteria and confidence interval;
- the generated report, JSON results, selected baseline, and accepted policy files.

The default run (30 seconds × 3 repetitions) requires roughly nine minutes of benchmark time, plus process startup overhead: four fixed candidates × three runs, followed by three selected-baseline runs and three adaptive runs. A dashboard runner log is saved to `live_data/comparison/dashboard_runner.log`. The dashboard only binds to localhost by default. The benchmark requires working user systemd and cgroup v2 CPU control; a dashboard button or report does not imply that a live run succeeded. Confirm the final status and saved measurement files.
