# Legacy baseline: `OS-Project-aegisos-core`

Verbatim copy of `src/` from the original `OS-Project-aegisos-core.zip` (interactive
"AegisOS Core v1.0" shell). It is kept **for reference and for porting useful logic only**.
It is not part of the new architecture and is not linked into `build/aegisos`.

- Build it on its own: `make legacy` (produces `build/legacy/aegis_legacy`).
- See `docs/LEGACY_ANALYSIS.md` for what each file does, what is retained, and known issues.
- The original `recovery.log` (two demo lines) was intentionally not copied; generated logs
  belong in `logs/` and are git-ignored.
- Do not edit these files. Port code into `src/` and delete the legacy copy when finished.
