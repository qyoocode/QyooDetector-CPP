#!/usr/bin/env python3
"""Prove zero and every single-bit location under strong perspective."""

from __future__ import annotations

import json
import re
import subprocess
import sys
from pathlib import Path


def main() -> int:
    detector = Path(sys.argv[1]).resolve()
    manifest_path = Path(sys.argv[2]).resolve()
    workspace = manifest_path.parents[3]
    detector_root = detector.parents[1]
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    selected = [
        case for case in manifest["cases"]
        if case["transform"]["transform_id"] == "perspective_strong"
        and (case["payload"]["payload_id"] == "zero"
             or case["payload"]["payload_id"].startswith("single_"))
    ]
    if len(selected) != 37:
        raise AssertionError(f"expected zero plus 36 single-bit cases, found {len(selected)}")
    failures = []
    for case in selected:
        image = workspace / case["image"]["path"]
        process = subprocess.run(
            [str(detector), str(image), "--normalization", "carrier-template"],
            cwd=detector_root, text=True, capture_output=True, check=False,
        )
        observed = re.findall(r"^Binary = ([01]{36})$", process.stdout, re.MULTILINE)
        expected = case["expected"]["raw_payload_bits"]
        if process.returncode != 0 or observed != [expected]:
            failures.append((case["case_id"], process.returncode, observed, expected))
    if failures:
        for failure in failures:
            print("FAIL:", failure, file=sys.stderr)
        return 1
    print("PASS: all-zero and all 36 single-bit strong-perspective carriers decode exactly")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
