# AegisOS Validation Record

Validation performed in the build environment for this packaged revision.

## Automated checks

- C11 simulator built with `-Wall -Wextra -Wpedantic -Wshadow -Werror`.
- Simulator unit tests passed.
- Live monitor tests passed for reading real `/proc` process state, snapshot enumeration, CSV/JSONL persistence, report generation, controller decision logic (mocked systemd), and failed-update handling (mocked systemd).
- Dashboard tests passed for latest-snapshot selection and JSONL event loading.
- HTTP smoke test returned HTTP 200 for `/`, `/api/data`, `/report`, `/api/report`, `/download/report`, and `/download/summary` against live process logs collected in the build environment.
- A fresh 400-run simulator matrix was generated and analyzed into `results/raw_results.csv`, `results/summary.csv`, `results/REPORT.md`, and plots.

## Not claimed as verified

The build environment did not have a working `systemd --user` manager. Therefore, the actual kernel/systemd `CPUWeight` property changes could not be exercised end-to-end here. Controller policy and error-handling paths are unit-tested with mocks, but actual control must be verified on the target Ubuntu/WSL system. Run:

```bash
python3 scripts/aegisos_live.py doctor
```

A real control session should create `controller_started` and one or more `cpu_weight_changed` events in `live_data/events-*.jsonl`. If only `cpu_weight_update_failed` events appear, the controller did not successfully apply the requested pair of weights. If no controller events appear, do not claim CPU allocation was changed.

## Evidence classification

- `results/REPORT.md`: simulated workloads only.
- `live_data/LIVE_REPORT.md`: actual process samples from the machine where AegisOS ran, if generated from that machine's logs.
- Dashboard screenshots and summaries should be labeled according to their underlying data source.
