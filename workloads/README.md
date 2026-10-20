# AegisOS Test Workloads

These bounded programs generate controlled workloads for testing process monitoring and supervision.

## Build

From the project root:

```bash
make workloads
```

Executables are created in `build/workloads/`.

## CPU workload

Run a CPU-intensive workload for a specified duration.

```bash
./build/workloads/cpu_worker <duty-percent> <duration-seconds>
```

- `duty-percent`: 1–100
- `duration-seconds`: 1–3600
- Example: `./build/workloads/cpu_worker 50 3`

The requested duty cycle is approximate; actual CPU utilization depends on system scheduling and load.

## Memory workload

Allocate memory incrementally, hold it briefly, and release it.

```bash
./build/workloads/memory_worker <maximum-MB> <interval-ms>
```

- `maximum-MB`: 1–256
- `interval-ms`: 10–10000 between allocations
- Example: `./build/workloads/memory_worker 4 100`

The workload touches allocated pages so memory use can be observed. It releases allocated memory before normal exit.

## Crash workload

Wait for a specified delay, then exit with a chosen status code.

```bash
./build/workloads/crash_worker <delay-seconds> <exit-code>
```

- `delay-seconds`: 0–3600
- `exit-code`: 0–255
- Example: `./build/workloads/crash_worker 1 7`

A nonzero exit code simulates an unsuccessful process termination. This version does not yet send a signal or repeat failures.

## Test conventions

- Exit code `0`: successful completion.
- Exit code `2`: invalid command-line arguments.
- Other exit codes: program-specific failure or requested exit status.

Keep test parameters bounded to avoid unnecessary CPU or memory consumption.
