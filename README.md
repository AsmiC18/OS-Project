# AegisOS — Linux Process Supervision and Self-Recovery System

An Operating Systems laboratory project: a lightweight, local C program for Linux that will
monitor registered applications, detect abnormal resource behaviour, supervise process
lifecycles, apply configurable recovery policies, and record measurable recovery outcomes,
with a browser dashboard served by a small C HTTP/JSON API.

> **Current state: project scaffold.** The folder structure, build system, documentation
> and test layout exist. **No monitoring, supervision, anomaly detection, recovery, API or
> dashboard functionality is implemented yet.** Everything below marked *planned* is a plan.

## The problem

Long-lived background applications on Linux can die unexpectedly, consume CPU or memory for
long periods, or fail repeatedly. Diagnosing and restarting them usually needs a person.
AegisOS aims to automate observation and (policy-limited) recovery, and to measure how well
that recovery works. See `docs/PROJECT_OVERVIEW.md`.

## Planned capabilities (none implemented yet)

| Area | Planned |
|---|---|
| Monitoring | Read `/proc`, CPU%/RSS/state per process, configurable interval, handle vanished PIDs |
| Supervision | Launch registered apps, track exit status, restart with exponential backoff and limits |
| Anomaly detection | Sustained-CPU, memory, crash and restart-loop rules with severity and explanations |
| Recovery | Policy-driven: alert-only, dry-run, restart, manual approval, cooldown, verification |
| Reporting | Structured incident log, filtering, CSV export, recovery statistics |
| API + dashboard | Localhost JSON API; HTML/CSS/vanilla-JS dashboard (7 sections) |
| Extension | cgroups v2 resource budgets (after the core works) |

Automated actions will only ever target applications registered with, or launched by, AegisOS.

## Layout

```
include/    public headers (one per module)          src/        module implementations + main.c
dashboard/  index.html, styles.css, app.js           tests/      C and shell tests (currently SKIP stubs)
workloads/  controlled test programs (stubs)         config/     policies.example.conf (draft)
data/ logs/ reports/   runtime output (git-ignored)  scripts/    build.sh, run.sh, clean.sh
docs/       design, plan, API, testing documents     legacy/     original baseline code (reference only)
```

## Build and run (works on the scaffold)

Requires Linux (or Ubuntu on WSL2), `gcc` (or `cc`) and `make`. For debugging: `gdb`.

```bash
make              # builds build/aegisos
./build/aegisos   # prints a scaffold banner only
make test         # runs tests; stubs report SKIP (exit 77), not PASS
make workloads    # builds workload stubs into build/workloads/
make legacy       # builds the original baseline shell, for reference
make clean
```

Wrappers: `scripts/build.sh`, `scripts/run.sh`, `scripts/clean.sh [--runtime]`.

**VS Code:** open the `AegisOS/` folder. Install the *C/C++* extension. `Ctrl+Shift+B` builds;
Run and Debug → "Debug aegisos" launches under gdb. On Windows use the *WSL* extension and
open the folder from inside Ubuntu (clone/extract under `~`, not `/mnt/c`, for speed).

## Design decisions at scaffold time

- **Legacy code** is preserved in `legacy/aegisos-core/` and analysed in `docs/LEGACY_ANALYSIS.md`.
  Its shell, scheduling simulators, IPC and synchronization demos are outside the new scope and
  are not linked into the main binary.
- **Modules:** `process` = parsing a single PID's `/proc` files; `monitor` = sampling schedule,
  CPU% deltas and history. They are separate on purpose; merge them if the code turns out tiny.
- **Persistence:** structured append-only files + CSV export first. SQLite stays optional.
- **Added beyond the requested tree:** `legacy/`, `docs/LEGACY_ANALYSIS.md`, `.vscode/c_cpp_properties.json`
  (IntelliSense include path).
- **LICENSE** is a placeholder; the team must choose one.

## Documentation

`docs/PROJECT_OVERVIEW.md` · `ARCHITECTURE.md` · `MODULE_DESIGN.md` · `API_SPECIFICATION.md` ·
`IMPLEMENTATION_PLAN.md` · `TESTING_PLAN.md` · `PERFORMANCE_EVALUATION.md` · `USER_MANUAL.md` ·
`TROUBLESHOOTING.md` · `TEAM_RESPONSIBILITIES.md` · `LEGACY_ANALYSIS.md`
