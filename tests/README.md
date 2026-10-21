# tests/

Current state: **stubs only.** Each `test_*.c` prints SKIP and returns 77; each `test_*.sh` does the same.
`make test` builds the C tests (linked against all non-`main` objects) and runs everything:
exit 0 = pass, 77 = skipped, anything else = fail.

| File | Will cover |
|---|---|
| `test_monitor.c` | `/proc` parsing, CPU% deltas, vanished PIDs |
| `test_supervisor.c` | restart, backoff, limits, cleanup |
| `test_anomaly.c` | sustained thresholds, spike rejection, severity |
| `test_recovery.c` | dry-run, cooldown, authorization, verification |
| `test_api.sh` | routes, validation, error codes |
| `test_integration.sh` | end-to-end with workloads |

See `docs/TESTING_PLAN.md`. Replace a stub only when the real test exists.
