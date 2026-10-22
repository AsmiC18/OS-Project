#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
PYTHON="$(command -v python3)"
UNIT_DIR="${XDG_CONFIG_HOME:-$HOME/.config}/systemd/user"
UNIT_FILE="$UNIT_DIR/aegisos-monitor.service"
mkdir -p "$UNIT_DIR" "$ROOT/live_data"

# Quote the working directory as a systemd unit-file value (not shell syntax).
SYSTEMD_ROOT="${ROOT//\\/\\\\}"
SYSTEMD_ROOT="${SYSTEMD_ROOT//\"/\\\"}"
cat > "$UNIT_FILE" <<EOF
[Unit]
Description=AegisOS Live Linux Process Monitor
After=default.target

[Service]
Type=simple
WorkingDirectory="$SYSTEMD_ROOT"
ExecStart=$PYTHON scripts/aegisos_live.py monitor --interval 1 --log-dir live_data --no-ui
Restart=on-failure
RestartSec=2

[Install]
WantedBy=default.target
EOF

systemctl --user daemon-reload
systemctl --user enable --now aegisos-monitor.service
printf 'AegisOS monitor service installed and started.\n'
printf 'Status: systemctl --user status aegisos-monitor.service\n'
printf 'Stop:   systemctl --user stop aegisos-monitor.service\n'
printf 'Logs:   %s/live_data/\n' "$ROOT"
printf 'Report: python3 "%s/scripts/summarize_live.py" --log-dir "%s/live_data"\n' "$ROOT" "$ROOT"
