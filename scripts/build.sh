#!/usr/bin/env bash
# Build AegisOS (wrapper around make). Extra arguments are passed to make.
set -euo pipefail
cd "$(dirname "$0")/.."
exec make "$@"
