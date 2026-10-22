import csv
import importlib.util
import tempfile
import unittest
from pathlib import Path

SCRIPT = Path(__file__).resolve().parents[1] / "scripts" / "aegisos_compare_live.py"
spec = importlib.util.spec_from_file_location("aegisos_compare_live", SCRIPT)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class LiveComparisonTests(unittest.TestCase):
    def test_percentile_and_confidence_interval(self):
        self.assertAlmostEqual(module.percentile([1, 2, 3, 4, 5], .95), 4.8)
        ci = module.difference_ci([10, 11, 12], [8, 9, 10])
        self.assertAlmostEqual(ci["mean_reduction_ms"], 2.0)
        self.assertLess(ci["low"], ci["mean_reduction_ms"])
        self.assertGreater(ci["high"], ci["mean_reduction_ms"])

    def test_background_progress_count(self):
        with tempfile.TemporaryDirectory() as td:
            path = Path(td) / "background.csv"
            with path.open("w", newline="") as f:
                writer = csv.writer(f); writer.writerow(["timestamp", "completed_chunks"])
                writer.writerow(["t1", 10]); writer.writerow(["t2", 24])
            self.assertEqual(module.background_units(path), 24)

    def test_probe_summary_uses_real_csv_rows(self):
        with tempfile.TemporaryDirectory() as td:
            path = Path(td) / "probe.csv"
            with path.open("w", newline="") as f:
                writer = csv.writer(f); writer.writerow(["timestamp", "probe_latency_ms", "iterations"])
                writer.writerow(["t1", 10, 1000]); writer.writerow(["t2", 20, 1000]); writer.writerow(["t3", 30, 1000])
            result = module.summarize_csv(path)
            self.assertEqual(result["samples"], 3)
            self.assertEqual(result["mean_ms"], 20)
            self.assertEqual(result["max_ms"], 30)


if __name__ == "__main__":
    unittest.main()
