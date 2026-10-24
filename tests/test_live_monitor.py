#!/usr/bin/env python3
import csv
import os
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
import aegisos_live


class LiveMonitorTests(unittest.TestCase):
    def test_reads_current_process(self):
        proc = aegisos_live.read_process(os.getpid())
        self.assertIsNotNone(proc)
        self.assertEqual(proc["pid"], os.getpid())
        self.assertGreaterEqual(proc["rss_kb"], 0)
        self.assertIn("cmdline", proc)

    def test_snapshot_contains_current_process(self):
        snapshot = aegisos_live.snapshot_processes()
        self.assertIn(os.getpid(), snapshot)

    def test_logger_persists_rows_and_events(self):
        with tempfile.TemporaryDirectory() as tmp:
            logger = aegisos_live.LiveLogger(Path(tmp))
            current = {123: {"pid": 123, "ppid": 1, "comm": "test", "cmdline": "test-process",
                             "state": "R", "cpu_ticks": 100, "rss_kb": 200, "cgroup": ""}}
            logger.process_rows(current, {}, 1.0)
            logger.event("test_event", value=1)
            logger.close()
            csv_path = next(Path(tmp).glob("processes-*.csv"))
            with csv_path.open(newline="", encoding="utf-8") as f:
                rows = list(csv.DictReader(f))
            self.assertEqual(len(rows), 1)
            self.assertEqual(rows[0]["pid"], "123")
            event_path = next(Path(tmp).glob("events-*.jsonl"))
            events = event_path.read_text(encoding="utf-8")
            self.assertIn("process_started_or_discovered", events)
            self.assertIn("test_event", events)


    def test_controller_changes_weights_from_live_scope_cpu(self):
        with tempfile.TemporaryDirectory() as tmp:
            logger = aegisos_live.LiveLogger(Path(tmp))
            controller = aegisos_live.AdaptiveController("aegisos-foreground-1.scope", "aegisos-background-1.scope")
            current = {
                101: {"pid": 101, "cgroup": "/user.slice/aegisos-foreground-1.scope"},
                202: {"pid": 202, "cgroup": "/user.slice/aegisos-background-1.scope"},
            }
            systemd_active = type("Result", (), {"returncode": 0, "stdout": "active\n"})()
            with patch("aegisos_live.subprocess.run", return_value=systemd_active), \
                 patch("aegisos_live.set_cpu_weight") as set_weight:
                controller.update(current, {101: 50.0, 202: 30.0}, logger)
            self.assertEqual(controller.weights, (770, 230))
            self.assertEqual(set_weight.call_args_list[0].args, ("aegisos-foreground-1.scope", 770))
            self.assertEqual(set_weight.call_args_list[1].args, ("aegisos-background-1.scope", 230))
            logger.close()
            event_text = next(Path(tmp).glob("events-*.jsonl")).read_text(encoding="utf-8")
            self.assertIn("cpu_weight_changed", event_text)

    def test_controller_logs_failed_weight_change_without_claiming_success(self):
        with tempfile.TemporaryDirectory() as tmp:
            logger = aegisos_live.LiveLogger(Path(tmp))
            controller = aegisos_live.AdaptiveController("aegisos-foreground-1.scope", "aegisos-background-1.scope")
            current = {
                101: {"pid": 101, "cgroup": "/user.slice/aegisos-foreground-1.scope"},
                202: {"pid": 202, "cgroup": "/user.slice/aegisos-background-1.scope"},
            }
            systemd_active = type("Result", (), {"returncode": 0, "stdout": "active\n"})()
            def fail_background(unit, weight):
                if unit == "aegisos-background-1.scope":
                    raise RuntimeError("mock permission denied")
            with patch("aegisos_live.subprocess.run", return_value=systemd_active), \
                 patch("aegisos_live.set_cpu_weight", side_effect=fail_background) as set_weight:
                controller.update(current, {101: 50.0, 202: 30.0}, logger)
            self.assertEqual(controller.weights, (500, 500))
            self.assertIn("failed", controller.message.lower())
            self.assertEqual(set_weight.call_count, 3)  # foreground attempt, background failure, rollback
            logger.close()
            event_text = next(Path(tmp).glob("events-*.jsonl")).read_text(encoding="utf-8")
            self.assertIn("cpu_weight_update_failed", event_text)

    def test_report_generation_from_live_log(self):
        with tempfile.TemporaryDirectory() as tmp:
            logger = aegisos_live.LiveLogger(Path(tmp))
            current = {321: {"pid": 321, "ppid": 1, "comm": "demo", "cmdline": "demo command",
                             "state": "S", "cpu_ticks": 5, "rss_kb": 1024, "cgroup": ""}}
            logger.process_rows(current, {}, 1.0)
            logger.close()
            result = subprocess.run([sys.executable, str(ROOT / "scripts/summarize_live.py"), "--log-dir", tmp],
                                    check=False, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertTrue((Path(tmp) / "LIVE_REPORT.md").exists())
            self.assertTrue((Path(tmp) / "live_process_summary.csv").exists())


if __name__ == "__main__":
    unittest.main()
