# SPDX-License-Identifier: MIT
import copy
import importlib.util
import math
import json
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location("benchmark", Path(__file__).parents[1] / "tools" / "benchmark.py")
benchmark = importlib.util.module_from_spec(spec)
spec.loader.exec_module(benchmark)


def record():
    return {"backend": "Helix", "backend_version": "1", "model_hash": "a" * 64,
            "precision": "fp16", "input_size": [1505, 847], "output_size": [2560, 1440],
            "dynamic_resolution_policy": "fixed", "shader_graph_hash": "b" * 64,
            "timing_boundary": "entire upscaler", "timing_type": "gpu",
            "scene": "traversal A", "game": "test game", "game_version": "test 1", "device": "A",
            "compiler": "compiler 1", "driver": "driver 1", "compilation_info": "flags A",
            "calibration": {"verified": True, "method": "timestamp frequency", "uncertainty_ms": 0.01},
            "sample_unit": "ms", "samples": [4.0] * 10}


class CompareTests(unittest.TestCase):
    def test_cross_device_and_environment_differences(self):
        a = record(); b = copy.deepcopy(a)
        b.update(device="B", compiler="compiler 2", samples=[2.0] * 10)
        result = benchmark.compare(a, b)
        self.assertTrue(result["comparable"])
        self.assertEqual(result["speedup"], 2)
        self.assertEqual(result["output_label"], "1440p")
        self.assertEqual(set(result["environment_differences"]), {"device", "compiler"})

    def test_unknown_never_matches_unknown(self):
        for field in ("model_hash", "timing_boundary", "input_size", "scene"):
            a = record(); b = record(); a[field] = b[field] = None
            result = benchmark.compare(a, b)
            self.assertFalse(result["comparable"], field)
            self.assertNotIn("speedup", result)

    def test_mismatches_refuse_comparison(self):
        for field, value in (("timing_type", "cpu"), ("timing_boundary", "isolated kernel"),
                             ("input_size", [1280, 720]), ("model_hash", "c" * 64),
                             ("output_size", [1920, 1080])):
            a = record(); b = record(); b[field] = value
            result = benchmark.compare(a, b)
            self.assertFalse(result["comparable"], field)
            self.assertNotIn("speedup", result)

    def test_invalid_samples(self):
        for samples in ([1.0] * 9, [0.0] * 10, [-1.0] * 10,
                        [math.nan] * 10, [math.inf] * 10, [True] * 10):
            a = record(); a["samples"] = samples
            self.assertFalse(benchmark.compare(a, record())["comparable"])

    def test_ticks_require_verified_frequency(self):
        a = record(); b = record()
        a.update(sample_unit="ticks", samples=[400000] * 10,
                 calibration={"verified": True, "method": "measured", "ticks_per_second": 100000000,
                              "uncertainty_ms": 0.01})
        self.assertTrue(benchmark.compare(a, b)["comparable"])
        a["calibration"].pop("ticks_per_second")
        self.assertFalse(benchmark.compare(a, b)["comparable"])
        a["calibration"]["ticks_per_second"] = 100000000
        a["calibration"]["verified"] = False
        self.assertFalse(benchmark.compare(a, b)["comparable"])

    def test_labels_use_exact_dimensions(self):
        for size, label in (([1920, 1080], "1080p"), ([3840, 2160], "4K"),
                            ([2560, 1439], "other")):
            a = record(); b = record(); a["output_size"] = b["output_size"] = size
            self.assertEqual(benchmark.compare(a, b)["output_label"], label)

    def test_graph_is_disclosed_as_environment(self):
        a = record(); b = record(); b["shader_graph_hash"] = "c" * 64
        result = benchmark.compare(a, b)
        self.assertTrue(result["comparable"])
        self.assertIn("shader_graph_hash", result["environment_differences"])

    def test_malformed_nested_and_numeric_metadata(self):
        for field, bad in (("model_hash", {"hash": "abc"}), ("scene", ["a"]),
                           ("input_size", [True, 847]), ("output_size", [2560, math.inf]),
                           ("backend_version", math.nan), ("shader_graph_hash", False),
                           ("calibration", {"verified": True, "method": {"nested": None}, "uncertainty_ms": 0})):
            a = record(); a[field] = bad
            self.assertFalse(benchmark.compare(a, record())["comparable"], field)

    def test_calibration_uncertainty(self):
        for value in (None, -1, math.nan, math.inf, True, 4):
            a = record(); a["calibration"]["uncertainty_ms"] = value
            self.assertFalse(benchmark.compare(a, record())["comparable"], repr(value))
        result = benchmark.compare(record(), record())
        self.assertLess(result["calibration_speedup_bounds"][0], 1)
        self.assertGreater(result["calibration_speedup_bounds"][1], 1)

    def test_duplicate_json_keys_rejected_at_any_depth(self):
        for text in ('{"model_hash":"a","model_hash":"a"}', '{"calibration":{"verified":true,"verified":true}}'):
            with self.assertRaises(ValueError):
                json.loads(text, object_pairs_hook=benchmark.unique_object)

    def test_invalid_hash_even_when_equal(self):
        for field in ("model_hash", "shader_graph_hash"):
            for value in ("abc", "A" * 64, "z" * 64, "a" * 63):
                a = record(); b = record(); a[field] = b[field] = value
                self.assertFalse(benchmark.compare(a, b)["comparable"])

    def test_extreme_numbers_do_not_crash_or_imply_speedup(self):
        for field, value in (("samples", [10**400] * 10), ("input_size", [10**400, 1]),
                             ("calibration", {"verified": True, "method": "x", "uncertainty_ms": 10**400})):
            a = record(); a[field] = value
            self.assertFalse(benchmark.compare(a, record())["comparable"])
        a = record(); b = record(); a["samples"] = [1e308] * 10; b["samples"] = [5e-324] * 10
        a["calibration"]["uncertainty_ms"] = b["calibration"]["uncertainty_ms"] = 0
        self.assertFalse(benchmark.compare(a, b)["comparable"])
        a = record(); a["sample_unit"] = "ticks"; a["samples"] = [5e-324] * 10
        a["calibration"].update(uncertainty_ms=0, ticks_per_second=1e308)
        self.assertFalse(benchmark.compare(a, record())["comparable"])

    def test_descriptive_percentiles_and_counts(self):
        a = record(); a["samples"] = list(range(1, 11))
        result = benchmark.compare(a, a)
        self.assertEqual((result["baseline_count"], result["baseline_p95_ms"], result["baseline_p99_ms"]), (10, 10, 10))
        self.assertIn("small samples", result["percentile_method"])


if __name__ == "__main__":
    unittest.main()
