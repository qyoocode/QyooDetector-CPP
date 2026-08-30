#!/usr/bin/env python3
"""Release-blocking regression test for the seven Task 09B wrong decodes."""

from __future__ import annotations

import hashlib
import json
import re
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
FIXTURE_ROOT = ROOT / "tests/fixtures/task09b_wrong_decode"
BINARY_PATTERN = re.compile(r"^(?:Projective )?Binary = ([01]{36})$", re.MULTILINE)
DIAGNOSTIC_PREFIX = "Diagnostics JSON = "
EXPECTED_REJECTION_FIELD = {
    "stress_historical_svg_204_02_0160": "structural_consistency_rejected",
    "stress_c_like_512_00_0800": "structural_consistency_rejected",
    "stress_c_like_512_24_0128": "structural_consistency_rejected",
    "stress_c_like_512_24_0400": "structural_consistency_rejected",
    "perspective_historical_svg_204_0220_xy_strong_01":
        "structural_alternative_rejected",
    "perspective_php_like_512_0360_x_strong_01":
        "raster_confirmation_attempted",
    "perspective_php_like_512_0360_xy_mild_00": "structural_consistency_rejected",
}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def main() -> int:
    detector = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "bin/qyoo_detector"
    manifest = json.loads((FIXTURE_ROOT / "manifest.json").read_text(encoding="utf-8"))
    failures = []
    for case in manifest["cases"]:
        image = FIXTURE_ROOT / case["image"]["path"]
        if sha256(image) != case["image"]["sha256"]:
            failures.append(f"{case['case_id']}: fixture SHA-256 mismatch")
            continue
        process = subprocess.run(
            [str(detector.resolve()), str(image.resolve()), "--normalization", "carrier-template",
             "--fallback-policy", "qualified", "--localization-policy", "native",
             "--diagnostics"],
            cwd=ROOT, capture_output=True, text=True, timeout=120, check=False)
        if process.returncode != 0:
            failures.append(f"{case['case_id']}: detector exited {process.returncode}")
            continue
        observed = BINARY_PATTERN.findall(process.stdout + process.stderr)
        unsafe = [bits for bits in observed if bits != case["expected_bits"]]
        if unsafe:
            failures.append(
                f"{case['case_id']}: accepted wrong {unsafe}; expected {case['expected_bits']}")
            continue
        diagnostic_lines = [
            line[len(DIAGNOSTIC_PREFIX):] for line in (process.stdout + process.stderr).splitlines()
            if line.startswith(DIAGNOSTIC_PREFIX)
        ]
        if len(diagnostic_lines) != 1:
            failures.append(f"{case['case_id']}: expected one diagnostics record")
            continue
        trace = json.loads(diagnostic_lines[0])
        template_records = [
            feature["carrier_template_diagnostics"] for feature in trace["features"]
            if feature["carrier_template_diagnostics"] is not None
        ]
        expected_field = EXPECTED_REJECTION_FIELD[case["case_id"]]
        mechanism_confirmed = any(record.get(expected_field) for record in template_records)
        if expected_field == "raster_confirmation_attempted":
            mechanism_confirmed = mechanism_confirmed and all(
                not record.get("raster_confirmation_passed")
                for record in template_records if record.get(expected_field))
        if not mechanism_confirmed:
            failures.append(
                f"{case['case_id']}: expected safety mechanism {expected_field} was absent")
    if failures:
        print("FAIL: Task 09B accepted-wrong regression")
        for failure in failures:
            print(f"  {failure}")
        return 1
    print(f"PASS: {manifest['case_count']} Task 09B fixtures produced no accepted-wrong payload")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
