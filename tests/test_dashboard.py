#!/usr/bin/env python3
"""Regression tests for the local dashboard's live-log reader."""
import csv
import json
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
import aegisos_dashboard


class DashboardTests(unittest.TestCase):
    def test_latest_snapshot_excludes_processes_that_disappeared(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            csv_path = root / "processes-session.csv"
            fields = ["timestamp", "pid", "ppid", "comm", "cmdline", "state", "cpu_percent", "rss_kb", "cgroup"]
            with csv_path.open("w", newline="", encoding="utf-8") as f:
                writer = csv.DictWriter(f, fieldnames=fields)
                writer.writeheader()
                writer.writerow({"timestamp": "2026-10-21T10:00:00+00:00", "pid": 100, "ppid": 1,
                                 "comm": "old", "cmdline": "old process", "state": "S",
                                 "cpu_percent": "15", "rss_kb": "1024", "cgroup": ""})
                writer.writerow({"timestamp": "2026-10-21T10:00:01+00:00", "pid": 200, "ppid": 1,
                                 "comm": "current", "cmdline": "current process", "state": "R",
                                 "cpu_percent": "20", "rss_kb": "2048", "cgroup": ""})
            data = aegisos_dashboard.load_data(root)
            self.assertEqual(data["process_count"], 1)
            self.assertEqual(data["processes"][0]["pid"], "200")
            self.assertEqual(data["processes"][0]["cpu_percent"], 20.0)
            self.assertEqual(data["processes"][0]["rss_mb"], 2.0)
            self.assertEqual(data["row_count"], 2)

    def test_bundled_project_results_are_loaded(self):
        project_root = ROOT
        data = aegisos_dashboard.load_project_data(project_root)
        self.assertEqual(data["total_runs"], 400)
        self.assertEqual(len(data["summary"]), 20)
        self.assertTrue(data["report_available"])
        adaptive = next(r for r in data["summary"] if r["scenario"] == "bursty" and r["policy"] == "adaptive")
        self.assertAlmostEqual(adaptive["mean_foreground_p95_ms"], 6.05)

    def test_events_are_read_from_jsonl(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "events-session.jsonl").write_text(
                json.dumps({"timestamp": "2026-10-21T10:00:00+00:00", "event": "test_event", "pid": 12}) + "\n",
                encoding="utf-8")
            data = aegisos_dashboard.load_data(root)
            self.assertEqual(data["event_count"], 1)
            self.assertEqual(data["events"][0]["event"], "test_event")


if __name__ == "__main__":
    unittest.main()
