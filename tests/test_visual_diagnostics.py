#!/usr/bin/env python3
"""Prove visual-debug exports are complete and decision-neutral."""

from __future__ import annotations

import json
import os
import re
import struct
import subprocess
import sys
import time
from pathlib import Path


PREFIX = "Diagnostics JSON = "


def run(detector: Path, image: Path, visual_directory: Path | None) -> subprocess.CompletedProcess[str]:
    command = [
        str(detector), str(image), "--normalization", "projective",
        "--fallback-policy", "qualified", "--diagnostics",
    ]
    if visual_directory is not None:
        visual_directory.mkdir(parents=True)
        command.extend(("--visual-debug-dir", str(visual_directory)))
    return subprocess.run(command, text=True, capture_output=True, check=False)


def observable(text: str) -> tuple[list[str], int]:
    bits = re.findall(r"^Binary = ([01]{36})$", text, re.MULTILINE)
    report = diagnostic(text)
    return bits, report["stages"]["accepted_candidate_count"]


def diagnostic(text: str) -> dict:
    lines = [line[len(PREFIX):] for line in text.splitlines() if line.startswith(PREFIX)]
    assert len(lines) == 1, f"expected one diagnostics line, found {len(lines)}"
    return json.loads(lines[0])


def png_size(path: Path) -> tuple[int, int]:
    data = path.read_bytes()[:24]
    assert data[:8] == b"\x89PNG\r\n\x1a\n"
    return struct.unpack(">II", data[16:24])


def main() -> int:
    if len(sys.argv) != 5:
        raise SystemExit(f"usage: {sys.argv[0]} DETECTOR ACCEPTED REJECTED TRASH_ROOT")
    detector, accepted, rejected, trash_root = map(Path, sys.argv[1:])
    run_root = trash_root / f"run-{time.time_ns()}-{os.getpid()}"

    accepted_control = run(detector, accepted, None)
    accepted_visual = run(detector, accepted, run_root / "accepted")
    assert accepted_control.returncode == accepted_visual.returncode == 0
    assert observable(accepted_control.stdout + accepted_control.stderr) == observable(
        accepted_visual.stdout + accepted_visual.stderr
    )
    accepted_report = diagnostic(accepted_visual.stdout + accepted_visual.stderr)
    assert accepted_report["visual_debug_geometry_included"] is True
    feature = next(item for item in accepted_report["features"] if item["accepted"])
    geometry = feature["visual_geometry"]
    assert geometry["detail_available"] is True
    assert geometry["original_contour_points"]
    assert geometry["distinctive_corner"] is not None
    assert len(geometry["corner_edges"]) == 2
    assert geometry["normalizations"]
    normalization = geometry["normalizations"][0]
    assert normalization["available"] is True
    assert len(normalization["observed_bits"]) == 36
    assert len(normalization["cells"]) == 36
    assert sorted(cell["bit_index"] for cell in normalization["cells"]) == list(range(36))
    patch = run_root / "accepted" / normalization["rectified_patch_file"]
    assert patch.is_file()
    assert png_size(patch) == (88, 88)

    rejected_control = run(detector, rejected, None)
    rejected_visual = run(detector, rejected, run_root / "rejected")
    assert rejected_control.returncode == rejected_visual.returncode == 0
    assert observable(rejected_control.stdout + rejected_control.stderr) == observable(
        rejected_visual.stdout + rejected_visual.stderr
    )
    rejected_report = diagnostic(rejected_visual.stdout + rejected_visual.stderr)
    detailed = [
        item for item in rejected_report["features"]
        if item["visual_geometry"]["detail_available"]
    ]
    assert detailed
    assert all(not item["accepted"] for item in detailed)
    assert any(item["visual_geometry"]["original_contour_points"] for item in detailed)

    print(f"PASS: visual diagnostics are complete and decision-neutral; evidence retained at {run_root}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
