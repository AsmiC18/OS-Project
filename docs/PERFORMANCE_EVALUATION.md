# Performance Evaluation

## Status

Two process-monitoring benchmarks have been run in Ubuntu under WSL:

1. Repeated samples of the current process.
2. Repeated samples across 4 and 16 child processes.

These benchmarks measure sampling performance. They do not measure the full AegisOS service's CPU or memory overhead, anomaly detection delay, or recovery performance.

## Metrics

| Metric | Definition | Status |
|---|---|---|
| Sampling throughput | Samples completed per elapsed second | Measured |
| Wall-clock time | Elapsed time for a benchmark trial | Measured |
| Benchmark-process CPU time | CPU time consumed by the benchmark process | Measured |
| Reported process memory | Memory reported in the last sample | Measured for single-process benchmark |
| Detection delay | Time from fault onset to alert | Not measured |
| Restart success rate | Verified successful restarts / restart attempts | Not measured |
| Recovery time | Failure detection to verified healthy state | Not measured |
| False alerts | Alerts without an injected fault | Not measured |
| Full-service monitoring overhead | AegisOS CPU and RSS with multiple monitored applications | Not measured |

## Method

- Environment: Ubuntu under WSL on Windows.
- Compiler: `cc`.
- Compiler flags: `-std=gnu11 -Wall -Wextra -Wshadow -O2 -g`.
- Monitoring implementation: `src/monitor.c` and `src/process.c`.
- Timing: `CLOCK_MONOTONIC` for elapsed time and `CLOCK_PROCESS_CPUTIME_ID` for benchmark-process CPU time.
- Raw results are saved in the `benchmarks/` directory.

Exact CPU, kernel, and host specifications were not recorded for these runs.

## Benchmark 1: Repeated Samples of One Process

Source: `benchmarks/monitor_benchmark.c`

Target: The benchmark's current process.

Trials: 10, with 1,000 samples per trial.

Raw results: `benchmarks/monitor_benchmark_results.csv`

Summary:

| Metric | Result |
|---|---:|
| Total samples | 10,000 |
| Mean wall time per trial | 0.009555 seconds |
| Mean benchmark-process CPU time per trial | 0.009546 seconds |
| Mean throughput | Approximately 109,916 samples/second |
| Minimum throughput | 88,292.82 samples/second |
| Maximum throughput | 170,288.65 samples/second |

These measurements describe repeated monitoring samples of one process in the tested environment.

## Benchmark 2: Multiple Processes

Source: `benchmarks/multi_process_monitor_benchmark.c`

The benchmark creates child processes that wait without intentionally consuming CPU. It samples each child for a fixed number of rounds and cleans up the children afterward.

Raw results: `benchmarks/multi_process_monitor_results.csv`

Ten trials were run for each configuration, with 100 rounds per process.

| Metric | 4 processes | 16 processes |
|---|---:|---:|
| Trials | 10 | 10 |
| Samples per trial | 400 | 1,600 |
| Mean throughput | 134,701.29 samples/second | 129,975.86 samples/second |
| Median throughput | 131,112.53 samples/second | 137,451.12 samples/second |
| Minimum throughput | 116,597.24 samples/second | 86,149.04 samples/second |
| Maximum throughput | 154,915.61 samples/second | 149,629.54 samples/second |

The measurements vary between trials. The results do not establish a guaranteed throughput limit or demonstrate the overhead of the complete AegisOS service.

## Reproduction

Build the single-process benchmark:

```bash
mkdir -p build/benchmarks

cc -Iinclude -std=gnu11 -Wall -Wextra -Wshadow -O2 -g \
  benchmarks/monitor_benchmark.c src/monitor.c src/process.c \
  -o build/benchmarks/monitor_benchmark
```

Run 10 trials:

```bash
printf 'samples,wall_seconds,cpu_seconds,samples_per_second,memory_mb\n' \
  > benchmarks/monitor_benchmark_results.csv

for i in $(seq 1 10); do
  ./build/benchmarks/monitor_benchmark 1000 \
    >> benchmarks/monitor_benchmark_results.csv || break
done
```

Build the multi-process benchmark:

```bash
mkdir -p build/benchmarks

cc -Iinclude -std=gnu11 -Wall -Wextra -Wshadow -O2 -g \
  benchmarks/multi_process_monitor_benchmark.c \
  src/monitor.c src/process.c \
  -o build/benchmarks/multi_process_monitor_benchmark
```

Run 10 trials for each configuration:

```bash
printf 'processes,rounds,total_samples,wall_seconds,cpu_seconds,samples_per_second\n' \
  > benchmarks/multi_process_monitor_results.csv

for i in $(seq 1 10); do
  ./build/benchmarks/multi_process_monitor_benchmark 4 100 \
    | tail -n 1 >> benchmarks/multi_process_monitor_results.csv || break
done

for i in $(seq 1 10); do
  ./build/benchmarks/multi_process_monitor_benchmark 16 100 \
    | tail -n 1 >> benchmarks/multi_process_monitor_results.csv || break
done
```

These commands overwrite the respective CSV result files when rerun.

## Limitations and Next Steps

1. Record exact host, CPU, and kernel details in future benchmark runs.
2. Evaluate sustained anomaly detection delay with controlled workloads.
3. Measure the complete service's CPU and memory overhead after integration.
4. Measure recovery time and restart success after recovery functionality is implemented.
5. Avoid interpreting these microbenchmarks as production-scale performance guarantees.

Only report measurements obtained from reproducible runs.
