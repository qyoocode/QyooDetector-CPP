#!/usr/bin/env python3
"""Prove in-process scaling, original-coordinate reporting, and early exit."""

from __future__ import annotations

import json
import re
import subprocess
import sys
from pathlib import Path


PREFIX = "Diagnostics JSON = "


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    if len(sys.argv) != 4:
        raise SystemExit(f"usage: {sys.argv[0]} DETECTOR IMAGE EXPECTED_BITS")
    detector, image = map(Path, sys.argv[1:3])
    expected = sys.argv[3]
    process = subprocess.run(
        [str(detector), str(image), "--diagnostics"],
        text=True, capture_output=True, check=False,
    )
    combined = process.stdout + process.stderr
    require(process.returncode == 0, "detector executes")
    observed = re.findall(r"^Binary = ([01]{36})$", combined, re.MULTILINE)
    require(observed == [expected], "production localization returns the known payload once")
    lines = [line[len(PREFIX):] for line in combined.splitlines()
             if line.startswith(PREFIX)]
    require(len(lines) == 1, "one structured trace is emitted")
    trace = json.loads(lines[0])
    multiscale = trace["multiscale"]
    require(multiscale["policy"] == "bounded-camera-v1", "production policy is named")
    require(multiscale["planned_attempt_count"] == 3, "work is bounded to three attempts")
    require(multiscale["attempted_scale_count"] == 1, "safe first-tier result exits early")
    require(multiscale["remaining_scales_skipped_after_success"] == 2,
            "trace reports skipped fallback work")
    attempt = multiscale["attempts"][0]
    require(attempt["label"] == "long-edge-1320", "first tier is deterministic")
    require(attempt["resampled_width"] == 1320, "resampled long edge is exact")
    require(attempt["payload_count"] == 1, "first tier produced one payload")
    mapped = multiscale["accepted_candidates_original_coordinates"][0]["bounds"]
    feature = next(value for value in trace["features"] if value["payload_extracted"])
    bounds = feature["bounds"]
    for original_key, scaled_key, scale_key in (
        ("min_x", "min_x", "input_scale_x"),
        ("max_x", "max_x", "input_scale_x"),
        ("min_y", "min_y", "input_scale_y"),
        ("max_y", "max_y", "input_scale_y"),
    ):
        expected_coordinate = bounds[scaled_key] / attempt[scale_key]
        require(abs(mapped[original_key] - expected_coordinate) < 0.1,
                f"{original_key} maps to original-image coordinates")
    print("PASS: production multiscale CLI early exit and coordinate restoration")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
