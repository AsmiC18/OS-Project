import importlib.util
import unittest
from pathlib import Path

SCRIPT = Path(__file__).resolve().parents[1] / "scripts" / "aegisos_evaluate_policy.py"
spec = importlib.util.spec_from_file_location("aegisos_evaluate_policy", SCRIPT)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


def sample(p95_base=12.0, p95_adaptive=9.0, bg_base=1000, bg_adaptive=970, ci_low=0.5):
    return {"baseline": {"p95_ms": p95_base}, "adaptive": {"p95_ms": p95_adaptive},
            "background_chunks_completed_per_run": {"baseline_mean": bg_base, "adaptive_mean": bg_adaptive},
            "mean_latency_reduction_95ci_ms": {"low": ci_low, "high": 2.0}}


class PolicyEvaluationTests(unittest.TestCase):
    def test_accepts_when_all_criteria_pass(self):
        self.assertEqual(module.evaluate(sample())["decision"], "ACCEPT")

    def test_rejects_when_background_progress_too_low(self):
        self.assertEqual(module.evaluate(sample(bg_adaptive=800))["decision"], "REJECT")

    def test_inconclusive_when_confidence_interval_crosses_zero(self):
        self.assertEqual(module.evaluate(sample(ci_low=-0.2))["decision"], "INCONCLUSIVE")

    def test_rejects_when_latency_improvement_too_small(self):
        self.assertEqual(module.evaluate(sample(p95_adaptive=11.5))["decision"], "REJECT")


if __name__ == "__main__":
    unittest.main()
