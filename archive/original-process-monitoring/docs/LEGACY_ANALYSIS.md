# Legacy Baseline Analysis (`OS-Project-aegisos-core.zip`)

Inspected: 24 files, ~2,060 lines of C under `src/`, plus `.gitignore` and a 2-line `recovery.log`.
There was no Makefile or documentation. The code was compiled once with `gcc` (builds; with `-O2` it warns about
unchecked `scanf` in `scheduler.c`). **It was not functionally tested during scaffolding;** observations below
come from reading the code.

## What it is
An interactive shell ("AegisOS Core v1.0") with built-in commands for process listing, kill/suspend/resume,
priorities, a resource monitor, a heuristic "health score", priority-based "healing", demo CPU schedulers,
a pipe IPC demo and a producer–consumer demo; unknown commands are run with `fork`/`execvp`.

## File-by-file disposition
| Legacy file | Purpose | Disposition in new design |
|---|---|---|
| `process.c/.h` (`list_processes`) | Enumerate `/proc/<pid>/status` | **Retain idea** → `process`/`monitor` enumeration |
| `monitor.c/.h` | CPU% from `/proc/stat` + `/proc/<pid>/stat` deltas; RSS; nice | **Port & rewrite** → `process` + `monitor` |
| `health.c/.h` | Heuristic score (CPU, mem%, state, nice); own `/proc` readers | **Replace** with explainable rules in `anomaly`; keep zombie/stopped-state idea |
| `process_control.c` | `kill`/`SIGSTOP`/`SIGCONT` on any PID | **Replace** → `supervisor`/`recovery`, registered apps only |
| `healing.c`, `autoheal.c`, `analysis.c` | Renice/suspend by score; verify nice value | **Replace** → `recovery` with policy, cooldown, verification (keep "verify after acting" idea) |
| `logger.c/.h` | Plain-text append to `recovery.log` | **Replace** → structured `logger` |
| `main.c` | Command shell, `parse_pid` validation | **Not carried over**; input-validation pattern reused; an optional CLI may come later |
| `scheduler.c`, `ipc.c`, `sync.c` | Textbook demos (FCFS/SJF/RR/priority, pipes, semaphores) | **Out of scope**; left in `legacy/` only |

## Issues to avoid repeating (from reading the code)
- Actions (`SIGSTOP`, `setpriority`) apply to any PID the user types; only `pid > 1` is checked.
- `analysis.c` treats `getpriority()==-1` as an error without checking `errno`, and applies a change before any decision.
- CPU% is defined differently in `monitor.c` (share of total ticks) and `health.c` (multiplied by CPU count).
- `monitor.c` uses fixed 500-PID arrays, a blocking `sleep(1)`, and unchecked `fgets`/`sscanf` results; it parses stat fields twice.
- `health.c` thresholds are hard-coded; one scan caps at 256 processes.
- `logger.c` writes to a cwd-relative file with `ctime()` text; not structured or filterable.
- `main.c` uses `system("clear")`; `scheduler.c`/`ipc.c` ignore return values of `scanf`/`pipe`.

## Worth keeping
Robust `strrchr(')')` handling of the `comm` field in `/proc/<pid>/stat`; `waitpid` retry on `EINTR`; input-length handling;
the habit of verifying an action's effect afterwards.
