#!/usr/bin/env python3
"""Freeze the nearest exact control selected for each Task 09B failure."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any


WORKSPACE = Path(__file__).resolve().parents[2]
DEFAULT_MANIFEST = WORKSPACE / "recovery/task-09b/nearest-controls/manifest.json"
DEFAULT_RESULTS = WORKSPACE / "recovery/task-09b/nearest-controls/native-results.json"
DEFAULT_OUTPUT = WORKSPACE / "recovery/task-09b/nearest-controls/selected-manifest.json"
SELECTION = {
    "stress_historical_svg_204_02_0160": "background_light",
    "stress_c_like_512_00_0800": "perspective_50pct",
    "stress_c_like_512_24_0128": "size_110pct",
    "stress_c_like_512_24_0400": "blur_none",
    "perspective_historical_svg_204_0220_xy_strong_01": "payload_dense18",
    "perspective_php_like_512_0360_x_strong_01": "size_110pct",
    "perspective_php_like_512_0360_xy_mild_00": "perspective_y_75pct",
}


def read_json(path: Path) -> Any:
    return json.loads(path.read_text(encoding="utf-8"))


def write_json(path: Path, value: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument("--results", type=Path, default=DEFAULT_RESULTS)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()
    manifest = read_json(args.manifest.resolve())
    results = read_json(args.results.resolve())
    by_id = {case["case_id"]: case for case in manifest["cases"]}
    result_by_id = {case["case_id"]: case for case in results["results"]}
    selected = []
    for base_id, variant in SELECTION.items():
        case_id = f"{base_id}__{variant}"
        case = by_id[case_id]
        result = result_by_id[case_id]
        if result["classification"] != "exact":
            raise AssertionError(f"selected control {case_id} is not exact")
        selected.append(case)
    output = {
        "schema": "org.qyoo.detector.task09b-selected-controls",
        "schema_version": 1,
        "purpose": "One measured nearest exact control for each frozen wrong-decode case.",
        "case_count": len(selected),
        "cases": selected,
    }
    write_json(args.output.resolve(), output)
    print(f"selected {len(selected)} exact controls")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
