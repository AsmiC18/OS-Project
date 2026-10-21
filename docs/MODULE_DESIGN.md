# Module Design (outline)

> Status: outline. Interfaces are not yet defined in headers.

| Module | Responsibility | Depends on | Open questions |
|---|---|---|---|
| `aegisos.h` | Version, shared types, error codes, limits | — | Error-return convention |
| `process` | Parse `/proc/<pid>/{stat,status,statm,cmdline}` into a struct; no policy | — | Handle comm names containing spaces/parentheses |
| `monitor` | Interval sampling, CPU% from tick deltas, bounded history | process | CPU% normalisation (per-core vs. total) — pick one and document |
| `supervisor` | Registry of apps; fork/exec; waitpid; exit status; backoff; restart limits; graceful stop | logger | Signal handling; process groups; zombie reaping |
| `anomaly` | Rules, sustain windows, severity, explanation text | monitor, policy | Threshold hysteresis to avoid flapping |
| `policy` | Parse/validate policy file; per-app settings | — | Config format (see `config/policies.example.conf`) |
| `recovery` | Decide/execute/verify actions; cooldown; approvals; authorization check | policy, supervisor, logger | Manual approval flow via API |
| `logger` | Structured event records, timestamps, rotation | — | Format (JSONL vs. CSV) |
| `reports` | Query/filter incidents, CSV export, statistics | logger | Time-range filter semantics |
| `api_server` | HTTP parsing, routing, JSON output, validation | all read APIs | Threading model; request size limits |

## Rules for adding modules
New module = `include/<name>.h` + `src/<name>.c`; the Makefile picks it up automatically.
Tests go in `tests/test_<name>.c`.
