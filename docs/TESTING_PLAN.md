# Testing Plan (outline)

> Status: no real tests exist. `make test` reports stubs as SKIP (exit 77).

## Levels
- **Unit (C):** parsers on fixture text, CPU% arithmetic, rule evaluation, policy validation.
- **Component:** supervisor with `workloads/crash_worker`, monitor against `workloads/cpu_worker`.
- **Integration (`tests/test_integration.sh`):** start AegisOS, run workloads, check incidents/CSV.
- **API (`tests/test_api.sh`):** routes, validation, error codes, loopback binding.

## Required scenarios
CPU-intensive · bounded memory growth · unexpected termination · repeated failure · monitoring overhead · recovery effectiveness ·
invalid input · permission failure · missing/vanished process · cleanup (no zombies, no stray children, no leaked fds).

## Conventions
Exit 0 = pass, 77 = skipped, other = fail. Tests must be deterministic or state their tolerance. Record real outputs; never hand-write results.

## Results log
| Date | Test | Result | Notes |
|---|---|---|---|
| _(empty — fill from real runs)_ | | | |
