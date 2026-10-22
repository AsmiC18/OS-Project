#!/usr/bin/env python3
"""Build a report from AegisOS live /proc snapshots and event logs."""
import argparse
import csv
import json
from collections import Counter, defaultdict
from pathlib import Path


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--log-dir", default="live_data", help="directory containing AegisOS live CSV/JSONL logs")
    args = ap.parse_args()
    root = Path(args.log_dir)
    files = sorted(root.glob("processes-*.csv"))
    if not files:
        ap.error(f"No processes-*.csv logs found in {root}; run AegisOS live monitoring first.")
    agg = defaultdict(lambda: {"samples": 0, "cpu_sum": 0.0, "cpu_max": 0.0, "rss_max": 0, "first": None, "last": None})
    sample_count = 0
    timestamps = []
    for path in files:
        with path.open(newline="", encoding="utf-8") as f:
            for row in csv.DictReader(f):
                sample_count += 1
                timestamps.append(row["timestamp"])
                key = (path.name, row["pid"], row["cmdline"])
                a = agg[key]
                cpu = float(row.get("cpu_percent") or 0)
                rss = int(float(row.get("rss_kb") or 0))
                a["samples"] += 1
                a["cpu_sum"] += cpu
                a["cpu_max"] = max(a["cpu_max"], cpu)
                a["rss_max"] = max(a["rss_max"], rss)
                a["first"] = a["first"] or row["timestamp"]
                a["last"] = row["timestamp"]
    event_counts = Counter()
    controller_events = []
    event_files = sorted(root.glob("events-*.jsonl"))
    for path in event_files:
        with path.open(encoding="utf-8") as f:
            for line in f:
                try:
                    event = json.loads(line)
                except json.JSONDecodeError:
                    continue
                event_counts[event.get("event", "unknown")] += 1
                if event.get("event") in ("cpu_weight_changed", "cpu_weight_update_failed", "controller_started", "controller_stopped"):
                    controller_events.append(event)
    summary_path = root / "live_process_summary.csv"
    ranked = sorted(agg.items(), key=lambda item: item[1]["cpu_sum"] / max(item[1]["samples"], 1), reverse=True)
    with summary_path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=["session_file", "pid", "command", "samples", "mean_cpu_percent", "max_cpu_percent", "max_rss_mb", "first_seen", "last_seen"])
        writer.writeheader()
        for (session, pid, cmd), a in ranked:
            writer.writerow({"session_file": session, "pid": pid, "command": cmd,
                             "samples": a["samples"], "mean_cpu_percent": f"{a['cpu_sum']/max(a['samples'],1):.2f}",
                             "max_cpu_percent": f"{a['cpu_max']:.2f}", "max_rss_mb": f"{a['rss_max']/1024:.2f}",
                             "first_seen": a["first"], "last_seen": a["last"]})
    report_path = root / "LIVE_REPORT.md"
    with report_path.open("w", encoding="utf-8") as f:
        f.write("# AegisOS Live Monitoring Report\n\n")
        f.write("This report was generated from real `/proc` snapshots collected by AegisOS on this machine. It is not a simulation report. CPU percentage is per logical CPU, so a multithreaded process may exceed 100%. Process IDs can be reused; rows are separated by log session.\n\n")
        f.write(f"- Snapshot files: {len(files)}\n- Event files: {len(event_files)}\n- Process-snapshot rows: {sample_count}\n")
        if timestamps:
            f.write(f"- First recorded sample: {min(timestamps)}\n- Last recorded sample: {max(timestamps)}\n")
        f.write("\n## Recorded event counts\n\n")
        if event_counts:
            for name, count in sorted(event_counts.items()):
                f.write(f"- `{name}`: {count}\n")
        else:
            f.write("No event records were found.\n")
        f.write("\n## Processes with highest mean sampled CPU\n\n| PID | Mean CPU % | Peak CPU % | Peak RSS MiB | Samples | Command |\n|---:|---:|---:|---:|---:|---|\n")
        for (session, pid, cmd), a in ranked[:20]:
            escaped_cmd = cmd.replace("|", "\\|")
            mean_cpu = a["cpu_sum"] / max(a["samples"], 1)
            f.write(f"| {pid} | {mean_cpu:.2f} | {a['cpu_max']:.2f} | {a['rss_max']/1024:.2f} | {a['samples']} | {escaped_cmd} |\n")
        f.write("\n## CPU allocation decisions\n\n")
        if controller_events:
            for event in controller_events:
                f.write(f"- {event.get('timestamp')}: `{event.get('event')}` — ")
                if event.get("event") == "cpu_weight_changed":
                    f.write(f"weights {event.get('old_weights')} → {event.get('new_weights')}; reason: {event.get('reason')}\n")
                elif event.get("event") == "cpu_weight_update_failed":
                    f.write(f"attempted {event.get('attempted_weights')}; retained {event.get('retained_weights')}; error: {event.get('reason')}\n")
                else:
                    f.write(f"{event.get('explanation', event.get('policy', 'controller event'))}\n")
        else:
            f.write("No live CPU-weight controller decisions were recorded in these logs. Monitor-only mode records processes but does not change CPU scheduling.\n")
        f.write("\n## Interpretation notes\n\n- CPU snapshots are periodic samples, not kernel tracing or application response-latency measurements.\n- `CPUWeight` is a relative cgroup scheduling weight, not a hard CPU quota or guaranteed CPU percentage.\n- In monitor-only mode, AegisOS does not change system scheduling. Live control applies only to the two commands explicitly launched in managed scopes.\n")
    print(f"Wrote {report_path} and {summary_path}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
