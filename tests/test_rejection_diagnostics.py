#!/usr/bin/env python3
"""Prove optional rejection diagnostics are structured and decision-neutral."""

from __future__ import annotations

import json
import re
import subprocess
import sys
import tempfile
from pathlib import Path


PREFIX = "Diagnostics JSON = "
REASON_CODES = {
    "none",
    "degenerate_bounds",
    "aspect_ratio",
    "feature_too_small",
    "feature_too_large",
    "distinctive_corner_not_found",
    "corner_edges_insufficient",
    "corner_angle",
    "circle_square_geometry",
    "outline_validation",
}
NORMALIZATION_OUTCOMES = {
    "not_attempted_candidate_rejected",
    "affine_payload_extracted",
    "payload_extraction_failed",
    "affine_normalization_unavailable",
    "not_attempted",
    "projective_refined_payload_extracted",
    "affine_fallback_payload_extracted",
    "projective_payload_extracted",
    "projective_fit_rejected",
    "insufficient_interior_correspondences_fallback_rejected",
    "projective_normalization_unavailable",
}


def run(detector: Path, image: Path, diagnostics: bool) -> subprocess.CompletedProcess[str]:
    command = [str(detector), str(image), "--normalization", "projective", "--verbose"]
    if diagnostics:
        command.append("--diagnostics")
    return subprocess.run(command, text=True, capture_output=True, check=False)


def observable(text: str) -> tuple[str | None, int, int]:
    bits = re.findall(r"^Binary = ([01]{36})$", text, re.MULTILINE)
    raw = re.findall(r"^Debug: Total features detected: (\d+)$", text, re.MULTILINE)
    accepted = re.findall(r"^Debug: Total Qyoo shapes detected: (\d+)$", text, re.MULTILINE)
    return (bits[-1] if bits else None, int(raw[-1]) if raw else 0, int(accepted[-1]) if accepted else 0)


def diagnostic(text: str) -> dict:
    lines = [line[len(PREFIX):] for line in text.splitlines() if line.startswith(PREFIX)]
    assert len(lines) == 1, f"expected one diagnostics line, found {len(lines)}"
    return json.loads(lines[0])


def prove_case(detector: Path, image: Path, should_accept: bool) -> None:
    control = run(detector, image, False)
    observed = run(detector, image, True)
    assert control.returncode == observed.returncode == 0
    assert observable(control.stdout + control.stderr) == observable(observed.stdout + observed.stderr)
    report = diagnostic(observed.stdout + observed.stderr)
    stages = report["stages"]
    assert report["image_loaded"] is True
    assert report["schema_version"] == 1
    assert stages["raw_feature_count"] == len(report["features"])
    assert sum(report["rejection_reason_counts"].values()) == stages["raw_feature_count"]
    assert set(report["rejection_reason_counts"]) == REASON_CODES
    assert stages["accepted_candidate_count"] == int(should_accept)
    assert sum(feature["accepted"] for feature in report["features"]) == int(should_accept)
    for feature in report["features"]:
        assert feature["rejection_reason"] in REASON_CODES
        assert feature["normalization_outcome"] in NORMALIZATION_OUTCOMES
        assert 0.0 <= feature["area_fraction"] <= 1.0
        assert 0.0 <= feature["aspect_ratio"] <= 1.0
        assert len(feature["near_corner_edges"]) == feature["near_corner_edge_count"]
        for edge in feature["near_corner_edges"]:
            assert edge["length_pixels"] >= 0.0
            assert 0.0 <= edge["angle_degrees"] <= 360.0


def main() -> int:
    if len(sys.argv) != 4:
        raise SystemExit(f"usage: {sys.argv[0]} DETECTOR ACCEPTED_PNG REJECTED_PNG")
    detector, accepted, rejected = map(Path, sys.argv[1:])
    prove_case(detector, accepted, True)
    prove_case(detector, rejected, False)
    with tempfile.NamedTemporaryFile() as invalid:
        invalid.write(b"not an image")
        invalid.flush()
        result = subprocess.run(
            [str(detector), invalid.name, "--diagnostics"],
            text=True,
            capture_output=True,
            check=False,
        )
        assert result.returncode != 0
        report = diagnostic(result.stdout + result.stderr)
        assert report["image_loaded"] is False
        assert report["failure_stage"] == "image_load_preparation"
    print("PASS: rejection diagnostics are structured, complete, and decision-neutral")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
