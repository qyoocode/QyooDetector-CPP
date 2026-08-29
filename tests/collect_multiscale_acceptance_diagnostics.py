#!/usr/bin/env python3
"""Collect compact, decision-relevant diagnostics for accepted scale observations.

The pre-implementation scale sweep intentionally stored only stage totals. This
companion tool reruns accepted observations and preserves the carrier/template
evidence needed to test conservative cross-scale qualification rules.
"""

from __future__ import annotations

import argparse
import concurrent.futures
import json
import re
import subprocess
from pathlib import Path
from typing import Any


WORKSPACE = Path(__file__).resolve().parents[2]
DEFAULT_MANIFEST = WORKSPACE / "recovery/task-09/generated-multiscale-matrix/manifest.json"
DEFAULT_OBSERVATIONS = WORKSPACE / "recovery/task-09/preimplementation-scale-evaluation/observations"
DEFAULT_CACHE = Path("/private/tmp/qyoo-task09-pillow-scale-cache")
DEFAULT_DETECTOR = WORKSPACE / "QyooDetector-CPP/bin/qyoo_detector"
DEFAULT_OUTPUT = WORKSPACE / "recovery/task-09/preimplementation-scale-evaluation/acceptance-diagnostics"
DIAGNOSTIC_PREFIX = "Diagnostics JSON = "


def read_json(path: Path) -> Any:
    return json.loads(path.read_text(encoding="utf-8"))


def write_json(path: Path, value: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


def parse_diagnostics(output: str) -> dict[str, Any]:
    for line in output.splitlines():
        if line.startswith(DIAGNOSTIC_PREFIX):
            return json.loads(line[len(DIAGNOSTIC_PREFIX):])
    raise ValueError("detector did not emit diagnostics JSON")


def compact_feature(feature: dict[str, Any]) -> dict[str, Any]:
    template = feature["carrier_template_diagnostics"]
    return {
        "feature_index": feature["feature_index"],
        "bounds": feature["bounds"],
        "model_close_fraction": feature["model_close_fraction"],
        "corner_angle_difference_degrees": feature["corner_angle_difference_degrees"],
        "projective_outline_rms_pixels": feature["projective_outline_rms_pixels"],
        "projective_outline_max_error_pixels": feature["projective_outline_max_error_pixels"],
        "carrier_projective_rms_pixels": feature["carrier_projective_rms_pixels"],
        "carrier_projective_max_error_pixels": feature["carrier_projective_max_error_pixels"],
        "carrier_template": template,
    }


def input_path(case: dict[str, Any], observation: dict[str, Any], cache: Path) -> Path:
    if abs(observation["scale"] - 1.0) < 1e-9:
        return WORKSPACE / case["image"]["path"]
    return cache / observation["scale_key"] / f"{case['case_id']}.jpg"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument("--observations", type=Path, default=DEFAULT_OBSERVATIONS)
    parser.add_argument("--cache", type=Path, default=DEFAULT_CACHE)
    parser.add_argument("--detector", type=Path, default=DEFAULT_DETECTOR)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--workers", type=int, default=4)
    parser.add_argument("--timeout", type=float, default=120.0)
    args = parser.parse_args()

    manifest = read_json(args.manifest.resolve())
    cases = {case["case_id"]: case for case in manifest["cases"]}
    tasks: list[tuple[dict[str, Any], dict[str, Any], Path]] = []
    for path in sorted(args.observations.resolve().glob("*/*.json")):
        observation = read_json(path)
        if not observation.get("observed_bits"):
            continue
        destination = args.output.resolve() / observation["scale_key"] / path.name
        if not destination.exists():
            tasks.append((cases[observation["case_id"]], observation, destination))
    print(f"pending accepted observations={len(tasks)}", flush=True)

    bits_pattern = re.compile(r"^(?:Projective )?Binary = ([01]{36})$", re.MULTILINE)

    def execute(task: tuple[dict[str, Any], dict[str, Any], Path]) -> dict[str, Any]:
        case, observation, destination = task
        image = input_path(case, observation, args.cache.resolve())
        process = subprocess.run(
            [str(args.detector.resolve()), str(image), "--normalization", "carrier-template",
             "--fallback-policy", "qualified", "--diagnostics"],
            cwd=WORKSPACE, capture_output=True, text=True, timeout=args.timeout, check=False)
        combined = process.stdout + process.stderr
        if process.returncode != 0:
            result = {"case_id": case["case_id"], "scale": observation["scale"],
                      "error": True, "returncode": process.returncode,
                      "output_tail": combined[-2000:]}
        else:
            report = parse_diagnostics(combined)
            accepted = [compact_feature(feature) for feature in report["features"]
                        if feature["accepted"] and feature["payload_extracted"]]
            observed = bits_pattern.findall(combined)
            result = {
                "case_id": case["case_id"],
                "scale": observation["scale"],
                "scale_key": observation["scale_key"],
                "expected_bits": case["expected"]["raw_payload_bits"],
                "observed_bits": observed,
                "exact": bool(observed) and all(
                    bits == case["expected"]["raw_payload_bits"] for bits in observed),
                "accepted_features": accepted,
                "error": False,
            }
        write_json(destination, result)
        return result

    with concurrent.futures.ThreadPoolExecutor(max_workers=args.workers) as executor:
        futures = [executor.submit(execute, task) for task in tasks]
        for index, future in enumerate(concurrent.futures.as_completed(futures), start=1):
            future.result()
            if index % 50 == 0 or index == len(futures):
                print(f"completed {index}/{len(futures)}", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
