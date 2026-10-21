#!/usr/bin/env bash
# Remove build output. Does NOT touch data/, logs/ or reports/ (use --runtime to clear them).
set -euo pipefail
cd "$(dirname "$0")/.."
make --no-print-directory clean
if [ "${1:-}" = "--runtime" ]; then
  find data logs reports -mindepth 1 ! -name .gitkeep -delete
  echo "Cleared generated files in data/, logs/, reports/."
fi
