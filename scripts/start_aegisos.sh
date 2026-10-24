#!/usr/bin/env bash
# One-command local launcher: runs monitor in background and dashboard in foreground.
set -euo pipefail
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
PYTHON="${PYTHON:-python3}"
INTERVAL="${AEGISOS_INTERVAL:-1}"
LOG_DIR="${AEGISOS_LOG_DIR:-live_data}"
mkdir -p "$LOG_DIR"
"$PYTHON" scripts/aegisos_live.py doctor
"$PYTHON" scripts/aegisos_live.py monitor --interval "$INTERVAL" --log-dir "$LOG_DIR" --no-ui &
MONITOR_PID=$!
cleanup() {
  kill -TERM "$MONITOR_PID" 2>/dev/null || true
  wait "$MONITOR_PID" 2>/dev/null || true
}
trap cleanup EXIT INT TERM
sleep 1
printf '\nAegisOS monitor is running (PID %s).\n' "$MONITOR_PID"
printf 'Dashboard: http://127.0.0.1:8765/\nLive report: http://127.0.0.1:8765/report\n'
printf 'Logs: %s/%s\n' "$ROOT" "$LOG_DIR"
printf 'Close with Ctrl+C; the monitor will stop and saved logs will remain.\n\n'
"$PYTHON" scripts/aegisos_dashboard.py --log-dir "$LOG_DIR" --host 127.0.0.1 --port 8765
