import csv
import importlib.util
import tempfile
import unittest
from pathlib import Path

SCRIPT = Path(__file__).resolve().parents[1] / "scripts" / "aegisos_compare_live.py"
spec = importlib.util.spec_from_file_location("aegisos_compare_live", SCRIPT)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
BENCHMARK = Path(__file__).resolve().parents[1] / "scripts" / "aegisos_live_benchmark.py"
bench_spec = importlib.util.spec_from_file_location("aegisos_live_benchmark", BENCHMARK)
benchmark = importlib.util.module_from_spec(bench_spec)
bench_spec.loader.exec_module(benchmark)


class LiveComparisonTests(unittest.TestCase):
    def test_fixed_baseline_candidates_are_explicit_and_include_legacy_reference(self):
        candidates = {c["name"]: (c["foreground"], c["background"]) for c in module.CANDIDATES}
        self.assertEqual(candidates["equal_500_500"], (500, 500))
        self.assertEqual(candidates["moderate_600_400"], (600, 400))
        self.assertEqual(candidates["strong_800_200"], (800, 200))
        self.assertEqual(candidates["legacy_600_100"], (600, 100))
        self.assertTrue(all(candidates[name][0] + candidates[name][1] == 1000 for name in ("equal_500_500", "moderate_600_400", "strong_800_200")))
        # Legacy 600/100 is preserved exactly for comparison with the original project, not normalized.

    def test_formula_based_weights_are_calculated_and_bounded(self):
        fg, bg, ratio, share = benchmark.calculate_weights(8.0, 8.0)
        self.assertEqual((fg, bg), (700, 300))
        self.assertEqual(fg + bg, 1000)
        self.assertAlmostEqual(ratio, 1.0)
        self.assertAlmostEqual(share, 0.70)
        high = benchmark.calculate_weights(20.0, 8.0)
        self.assertEqual(high[0], 900)
        self.assertEqual(high[1], 100)
        low = benchmark.calculate_weights(1.0, 8.0)
        self.assertEqual(low[0], 550)
        self.assertEqual(low[1], 450)

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
