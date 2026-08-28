#!/usr/bin/env python3
"""Prove a valid corner-edge pair is used even when it is not the longest two."""

from __future__ import annotations

import json
import re
import subprocess
import sys
from itertools import combinations
from pathlib import Path


PREFIX = "Diagnostics JSON = "


def run(detector: Path, image: Path) -> tuple[dict, list[str]]:
    result = subprocess.run(
        [str(detector), str(image), "--normalization", "projective",
         "--fallback-policy", "qualified", "--diagnostics"],
        text=True, capture_output=True, check=True,
    )
    document = json.loads(next(
        line[len(PREFIX):] for line in result.stdout.splitlines() if line.startswith(PREFIX)
    ))
    bits = re.findall(r"^Binary = ([01]{36})$", result.stdout, re.MULTILINE)
    return document, bits


def angle_difference(left: float, right: float) -> float:
    difference = abs(left - right)
    if difference > 180.0:
        difference -= 180.0
    return difference


def main() -> int:
    if len(sys.argv) != 5:
        raise SystemExit(f"usage: {sys.argv[0]} DETECTOR BLURRED_PNG EXPECTED_BITS NEGATIVE_PNG")
    detector, blurred, expected_bits, negative = Path(sys.argv[1]), Path(sys.argv[2]), sys.argv[3], Path(sys.argv[4])
    report, results = run(detector, blurred)
    size_candidates = [feature for feature in report["features"] if feature["size_check_passed"]]
    assert len(size_candidates) == 1
    edges = size_candidates[0]["near_corner_edges"]
    qualifying_pairs = [
        (left, right) for left, right in combinations(edges, 2)
        if 65.0 < angle_difference(left["angle_degrees"], right["angle_degrees"]) < 125.0
    ]
    assert qualifying_pairs, "the proving candidate must contain an historically valid corner pair"
    assert report["stages"]["corner_geometry_pass_count"] == 1
    # The isolated corner-pair repair does not relax the next outline-model
    # stage; this blurred carrier remains rejected there.
    assert report["stages"]["accepted_candidate_count"] == 0
    assert size_candidates[0]["rejection_reason"] == "outline_validation"
    assert expected_bits not in results

    negative_report, negative_results = run(detector, negative)
    assert negative_report["stages"]["accepted_candidate_count"] == 0
    assert not negative_results
    print("PASS: the longest historically valid corner pair reaches outline validation without accepting the focused negative")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
