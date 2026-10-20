# workloads/

Controlled programs AegisOS will supervise during tests and benchmarks. Current state: **stubs that exit 77.**

| Source | Planned behaviour |
|---|---|
| `cpu_worker.c` | Busy loop with configurable duty cycle and duration |
| `memory_worker.c` | Allocates memory at a bounded rate up to a fixed ceiling |
| `crash_worker.c` | Exits with a code or signal after a delay; optionally repeats |

Build: `make workloads` → `build/workloads/`. Workloads must be bounded (self-terminating or capped) so a mistake cannot exhaust the machine.
