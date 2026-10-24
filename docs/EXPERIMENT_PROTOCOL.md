# Experimental Protocol

1. Build and test with `make clean all test`.
2. Run the default matrix with `make experiment` or a smaller pilot using the CLI.
3. Keep raw CSV results unchanged and record the source version, compiler, command, and run count.
4. Use `steady` and `sustained` only to select a static baseline under the documented rule.
5. Treat `bursty` and `mixed` as held-out evaluation scenarios.
6. Use matched seeds across policies. Do not tune controller parameters using held-out results.
7. Report foreground p95 latency and background on-time completion together; neither metric alone describes the trade-off.
8. Interpret paired bootstrap intervals cautiously. They quantify variation across simulated seeds, not simulator-model uncertainty.
9. Report negative or mixed results honestly. Do not claim adaptive superiority unless the held-out comparisons support it.
