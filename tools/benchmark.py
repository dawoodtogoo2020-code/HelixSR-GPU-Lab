# SPDX-License-Identifier: MIT
"""Validate and compare measured upscaler latency records. No predictions are made.

Usage: python benchmark.py baseline.json candidate.json
Each file contains one JSON object; samples are individual observations, not percentiles.
Matching fields and verified calibration are caller attestations, not independently
proven by this tool. Calibration bounds do not quantify sampling or scene uncertainty.
"""
import json
import math
import re
import sys

MIN_SAMPLES = 10
WORKLOAD = ("backend", "backend_version", "model_hash", "precision", "input_size",
            "output_size", "dynamic_resolution_policy",
            "timing_boundary", "timing_type", "scene", "game", "game_version")
ENVIRONMENT = ("device", "compiler", "driver", "compilation_info", "shader_graph_hash")
REQUIRED = WORKLOAD + ENVIRONMENT + ("calibration", "sample_unit", "samples")
LABELS = {(1920, 1080): "1080p", (2560, 1440): "1440p", (3840, 2160): "4K"}


def finite_number(value):
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        return False
    try:
        return math.isfinite(value)
    except OverflowError:
        return False


def known(value):
    if value is None:
        return False
    if isinstance(value, bool):
        return True
    if isinstance(value, str):
        return bool(value.strip()) and value.strip().lower() not in {"unknown", "n/a", "none"}
    if isinstance(value, (list, dict)):
        return bool(value) and all(known(v) for v in (value.values() if isinstance(value, dict) else value))
    return finite_number(value)


def valid_size(size):
    return (isinstance(size, list) and len(size) == 2 and
            all(isinstance(n, int) and not isinstance(n, bool) and 0 < n <= 0xffffffff for n in size))


def validate(record, side):
    reasons = []
    if not isinstance(record, dict):
        return [f"{side}: record must be an object"], None
    for key in REQUIRED:
        if key not in record or not known(record[key]):
            reasons.append(f"{side}: missing or unknown {key}")
    for key in WORKLOAD + ENVIRONMENT:
        if key not in ("input_size", "output_size") and not isinstance(record.get(key), str):
            reasons.append(f"{side}: {key} must be a nonempty string")
    for key in ("model_hash", "shader_graph_hash"):
        if not isinstance(record.get(key), str) or not re.fullmatch(r"[0-9a-f]{64}", record[key]):
            reasons.append(f"{side}: {key} must be a lowercase SHA-256 hex digest")
    for key in ("input_size", "output_size"):
        if not valid_size(record.get(key)):
            reasons.append(f"{side}: {key} must be [positive integer width, height]")
    timing = record.get("timing_type")
    unit = record.get("sample_unit")
    if timing not in ("gpu", "cpu"):
        reasons.append(f"{side}: timing_type must be gpu or cpu")
    if unit not in ("ms", "ticks") or (timing == "cpu" and unit != "ms"):
        reasons.append(f"{side}: unsupported timing unit")
    calibration = record.get("calibration")
    if not isinstance(calibration, dict) or calibration.get("verified") is not True:
        reasons.append(f"{side}: timing calibration must be verified")
    if not isinstance(calibration, dict) or not known(calibration.get("method")):
        reasons.append(f"{side}: calibration method is required")
    elif not isinstance(calibration["method"], str):
        reasons.append(f"{side}: calibration method must be a string")
    uncertainty = calibration.get("uncertainty_ms") if isinstance(calibration, dict) else None
    if not finite_number(uncertainty) or uncertainty < 0:
        reasons.append(f"{side}: calibration uncertainty_ms must be finite and nonnegative")
    frequency = calibration.get("ticks_per_second") if isinstance(calibration, dict) else None
    if unit == "ticks" and (not finite_number(frequency) or frequency <= 0):
        reasons.append(f"{side}: tick frequency must be finite and positive")
    samples = record.get("samples")
    if (not isinstance(samples, list) or len(samples) < MIN_SAMPLES or
        any(not finite_number(x) or x <= 0
            for x in samples)):
        reasons.append(f"{side}: samples require at least {MIN_SAMPLES} finite positive observations")
    if reasons:
        return reasons, None
    try:
        factor = 1000 / frequency if unit == "ticks" else 1
        values = [x * factor for x in samples]
    except (OverflowError, ZeroDivisionError):
        return [f"{side}: sample conversion overflow"], None
    if not finite_number(factor) or factor <= 0 or any(not finite_number(x) or x <= 0 for x in values):
        return [f"{side}: converted samples are invalid"], None
    if uncertainty >= median(values):
        return [f"{side}: calibration uncertainty is at least the median"], None
    return [], values


def median(values):
    ordered = sorted(values)
    middle = len(ordered) // 2
    return ordered[middle] if len(ordered) % 2 else ordered[middle - 1] + (ordered[middle] - ordered[middle - 1]) / 2


def percentile(values, percentile_value):
    ordered = sorted(values)
    return ordered[math.ceil(len(ordered) * percentile_value / 100) - 1]


def compare(baseline, candidate):
    reasons_a, samples_a = validate(baseline, "baseline")
    reasons_b, samples_b = validate(candidate, "candidate")
    reasons = reasons_a + reasons_b
    differences = {}
    if isinstance(baseline, dict) and isinstance(candidate, dict):
        for field in WORKLOAD:
            a, b = baseline.get(field), candidate.get(field)
            if not known(a) or not known(b):
                reasons.append(f"{field}: unknown values cannot establish equivalence")
            elif a != b:
                reasons.append(f"{field}: workload or measurement mismatch")
        for field in ENVIRONMENT:
            a, b = baseline.get(field), candidate.get(field)
            if a != b or not known(a) or not known(b):
                differences[field] = {"baseline": a, "candidate": b}
    result = {"comparable": not reasons, "reasons": reasons, "environment_differences": differences}
    if isinstance(baseline, dict) and isinstance(candidate, dict):
        result["measurement_differences"] = {
            key: {"baseline": baseline.get(key), "candidate": candidate.get(key)}
            for key in ("sample_unit", "calibration") if baseline.get(key) != candidate.get(key)
        }
    if not reasons:
        a, b = median(samples_a), median(samples_b)
        ua, ub = baseline["calibration"]["uncertainty_ms"], candidate["calibration"]["uncertainty_ms"]
        try:
            ratios = [a / b, (a - ua) / (b + ub), (a + ua) / (b - ub)]
        except (OverflowError, ZeroDivisionError):
            ratios = []
        if len(ratios) != 3 or any(not finite_number(x) or x <= 0 for x in ratios):
            result["comparable"] = False
            result["reasons"].append("calculated speedup or bounds are not representable")
            return result
        result.update({"baseline_median_ms": a, "candidate_median_ms": b,
                       "baseline_count": len(samples_a), "candidate_count": len(samples_b),
                       "baseline_p95_ms": percentile(samples_a, 95), "baseline_p99_ms": percentile(samples_a, 99),
                       "candidate_p95_ms": percentile(samples_b, 95), "candidate_p99_ms": percentile(samples_b, 99),
                       "percentile_method": "nearest rank; descriptive only, small samples do not establish tail behavior",
                       "speedup": ratios[0],
                       "calibration_speedup_bounds": ratios[1:],
                       "output_label": LABELS.get(tuple(baseline["output_size"]), "other")})
    return result


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def reject_constant(value):
    raise ValueError(f"non-finite JSON number: {value}")


def safe_output(value):
    if isinstance(value, float) and not math.isfinite(value):
        return "non-finite"
    if isinstance(value, int) and not isinstance(value, bool) and not finite_number(value):
        return "out-of-range integer"
    if isinstance(value, dict):
        return {k: safe_output(v) for k, v in value.items()}
    if isinstance(value, list):
        return [safe_output(v) for v in value]
    return value


def main(argv=None):
    argv = sys.argv[1:] if argv is None else argv
    if argv in (["--help"], ["-h"]):
        print("Usage: benchmark.py baseline.json candidate.json\nCompare matched recorded workloads. See docs/benchmark.md for the input format.")
        return 0
    if len(argv) != 2:
        print("Usage: benchmark.py baseline.json candidate.json", file=sys.stderr)
        return 2
    try:
        with open(argv[0], encoding="utf-8") as f:
            baseline = json.load(f, object_pairs_hook=unique_object, parse_constant=reject_constant)
        with open(argv[1], encoding="utf-8") as f:
            candidate = json.load(f, object_pairs_hook=unique_object, parse_constant=reject_constant)
        result = compare(baseline, candidate)
        print(json.dumps(safe_output(result), indent=2, allow_nan=False))
        return 0 if result["comparable"] else 1
    except (OSError, ValueError, TypeError) as exc:
        print(json.dumps({"comparable": False, "reasons": [str(exc)]}), file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
