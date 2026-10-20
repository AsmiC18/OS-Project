#!/usr/bin/env bash
# Build if needed, then run AegisOS. Arguments are passed to the program.
set -euo pipefail
cd "$(dirname "$0")/.."
make --no-print-directory all
exec ./build/aegisos "$@"
