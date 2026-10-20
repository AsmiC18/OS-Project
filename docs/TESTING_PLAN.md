# Testing Plan

## Current Status

The project has implemented unit tests for process information reading, process monitoring, anomaly detection, and CSV reporting.

Latest recorded run:

```text
tests: 4 passed, 4 skipped (not implemented), 0 failed
```

The CPU, memory, and crash workload programs compile successfully. Their individual behavior has been tested manually with valid and invalid arguments.

## Test Levels

- **Unit tests:** Process information parsing, monitoring samples and history, sustained anomaly detection, and CSV reporting.
- **Component tests:** Supervisor behavior with controlled workloads and recovery behavior after process failure. These tests remain incomplete.
- **Integration tests:** Start AegisOS, run workloads, and verify incidents and CSV output. Not yet implemented.
- **API tests:** Validate routes, input validation, error codes, and loopback binding. Not yet implemented.

## Required Scenarios

- CPU-intensive workload
- Bounded memory growth
- Unexpected process termination
- Repeated failure and recovery
- Monitoring overhead
- Recovery effectiveness
- Invalid input
- Permission failure
- Missing or vanished process
- Cleanup, including avoiding zombies, stray children, and leaked file descriptors

## Test Conventions

- Exit code `0`: passed.
- Exit code `77`: skipped because the test is not implemented.
- Any other nonzero exit code: failed.
- Tests should be deterministic or document acceptable tolerances.
- Record actual outputs and measurements; do not invent results.

## Latest Test Results

Command:

```bash
make clean && make && make test && make workloads
```

| Test suite | Result |
|---|---|
| Anomaly detection | Passed |
| Process monitoring | Passed |
| Process information | Passed |
| CSV reporting | Passed |
| Recovery | Skipped — not implemented |
| Supervisor | Skipped — not implemented |
| API | Skipped — not implemented |
| Integration | Skipped — not implemented |
| Workload compilation | Passed |

Total: 4 test suites passed, 4 skipped, 0 failed.

## Limitations

The current test run does not demonstrate successful recovery, supervisor operation, API behavior, or end-to-end integration. Performance measurements have not yet been collected.
