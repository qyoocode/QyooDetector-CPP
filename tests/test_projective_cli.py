#!/usr/bin/env python3
"""Focused known-answer proof for projective normalization shadow mode."""

from __future__ import annotations

import json
import re
import subprocess
import sys
from pathlib import Path


def main() -> int:
    if len(sys.argv) != 4:
        raise SystemExit("usage: test_projective_cli.py DETECTOR MANIFEST CONTROL_RESULTS_JSONL")
    detector = Path(sys.argv[1]).resolve()
    manifest_path = Path(sys.argv[2]).resolve()
    control_path = Path(sys.argv[3]).resolve()
    workspace = manifest_path.parents[3]
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    cases = {case["case_id"]: case for case in manifest["cases"]}
    control = {item["case_id"]: item for item in map(json.loads, control_path.read_text(encoding="utf-8").splitlines())}
    selected = ("perspective_1", "perspective_2", "perspective_3", "perspective_4", "background_gradient")
    failures = []
    for case_id in selected:
        case = cases[case_id]
        image = workspace / case["image"]["path"]
        process = subprocess.run(
            [str(detector), str(image), "--normalization", "shadow", "--verbose"],
            cwd=detector.parents[1],
            text=True,
            capture_output=True,
            check=False,
        )
        output = process.stdout + process.stderr
        affine = re.findall(r"^Binary = ([01]{36})\s*$", output, re.MULTILINE)
        projective = re.findall(r"^Projective Binary = ([01]{36})\s*$", output, re.MULTILINE)
        expected = case["expected"]["raw_payload_bits"]
        frozen_affine = control[case_id]["carrier_decode"]["raw_36bit_results"]
        if process.returncode != 0:
            failures.append(f"{case_id}: exit {process.returncode}")
        if affine != frozen_affine:
            failures.append(f"{case_id}: affine {affine} != frozen {frozen_affine}")
        if projective != [expected]:
            failures.append(f"{case_id}: projective {projective} != expected {[expected]}")
        default_process = subprocess.run(
            [str(detector), str(image), "--verbose"],
            cwd=detector.parents[1],
            text=True,
            capture_output=True,
            check=False,
        )
        default_output = default_process.stdout + default_process.stderr
        default_bits = re.findall(r"^Binary = ([01]{36})\s*$", default_output, re.MULTILINE)
        if default_process.returncode != 0:
            failures.append(f"{case_id}: default exit {default_process.returncode}")
        if default_bits != [expected]:
            failures.append(f"{case_id}: default {default_bits} != expected {[expected]}")
    if failures:
        print("\n".join(f"FAIL: {failure}" for failure in failures), file=sys.stderr)
        return 1
    print("PASS: frozen affine control retained; shadow and default projective decodes exact")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
