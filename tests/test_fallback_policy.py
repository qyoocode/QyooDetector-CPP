#!/usr/bin/env python3
"""Prove the three projective fallback policies on sparse known-answer cases."""

from __future__ import annotations

import json
import re
import subprocess
import sys
from pathlib import Path


PREFIX = "Diagnostics JSON = "


def cases(path: Path) -> dict[str, dict]:
    manifest = json.loads(path.read_text(encoding="utf-8"))
    return {case["case_id"]: case for case in manifest["cases"]}


def run(detector: Path, image: Path, policy: str) -> tuple[list[str], dict]:
    process = subprocess.run(
        [str(detector), str(image), "--normalization", "projective",
         "--fallback-policy", policy, "--diagnostics"],
        text=True,
        capture_output=True,
        check=False,
    )
    assert process.returncode == 0
    bits = re.findall(r"^Binary = ([01]{36})$", process.stdout, re.MULTILINE)
    diagnostic_lines = [line[len(PREFIX):] for line in process.stdout.splitlines() if line.startswith(PREFIX)]
    assert len(diagnostic_lines) == 1
    return bits, json.loads(diagnostic_lines[0])


def image(workspace: Path, case: dict) -> Path:
    path = Path(case["image"]["path"])
    return path if path.is_absolute() else workspace / path


def main() -> int:
    if len(sys.argv) != 4:
        raise SystemExit(f"usage: {sys.argv[0]} DETECTOR FROZEN_MANIFEST SWEEP_MANIFEST")
    detector = Path(sys.argv[1]).resolve()
    frozen_manifest = Path(sys.argv[2]).resolve()
    sweep_manifest = Path(sys.argv[3]).resolve()
    workspace = frozen_manifest.parents[3]
    frozen = cases(frozen_manifest)
    sweep = cases(sweep_manifest)

    frontal = frozen["expanded_raw_single_set_bit_17"]
    mild = sweep["sweep_single17_horizontal_064px"]
    corrupt = sweep["sweep_single17_horizontal_080px"]
    expected = frontal["expected"]["raw_payload_bits"]
    assert mild["expected"]["raw_payload_bits"] == corrupt["expected"]["raw_payload_bits"] == expected

    for case in (frontal, mild):
        legacy, _ = run(detector, image(workspace, case), "legacy-affine")
        qualified, report = run(detector, image(workspace, case), "qualified")
        rejected, rejected_report = run(detector, image(workspace, case), "reject")
        assert expected in legacy
        assert expected in qualified
        assert not rejected
        assert report["stages"]["affine_fallback_count"] == 1
        assert rejected_report["stages"]["affine_fallback_rejected_count"] == 1

    legacy, _ = run(detector, image(workspace, corrupt), "legacy-affine")
    qualified, qualified_report = run(detector, image(workspace, corrupt), "qualified")
    rejected, rejected_report = run(detector, image(workspace, corrupt), "reject")
    assert legacy and expected not in legacy
    assert not qualified and not rejected
    assert qualified_report["stages"]["affine_fallback_rejected_count"] == 1
    assert rejected_report["stages"]["affine_fallback_rejected_count"] == 1
    accepted = [feature for feature in qualified_report["features"] if feature["accepted"]]
    assert len(accepted) == 1
    assert abs(accepted[0]["corner_angle_difference_degrees"] - 90.0) > 2.0
    print("PASS: legacy, reject, and 2-degree qualified fallback policies are distinct and safe")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
