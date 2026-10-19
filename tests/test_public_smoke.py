#!/usr/bin/env python3
"""Verify a positive raw-payload fixture and a blank rejection through the CLI."""
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
fixtures = root / "tests/fixtures/public-smoke"
manifest = json.loads((fixtures / "manifest.json").read_text())
counts = {"accepted_correct": 0, "accepted_wrong": 0, "rejected_positive": 0,
          "correct_negative": 0, "false_positive": 0}
for fixture in manifest["fixtures"]:
    image = fixtures / fixture["file"]
    if hashlib.sha256(image.read_bytes()).hexdigest() != fixture["sha256"]:
        raise SystemExit(f"FAIL: fixture digest differs: {image.name}")
    run = subprocess.run([str(root / "bin/qyoo_detector"), str(image)],
                         cwd=root, capture_output=True, text=True, timeout=30)
    if run.returncode != 0:
        raise SystemExit(f"FAIL: detector error for {image.name} (exit {run.returncode})")
    values = [int(value) for value in re.findall(r"^Qyoo value = (\d+)$", run.stdout, re.M)]
    expected = fixture["expected_payload"]
    if expected is None:
        outcome = "false_positive" if values else "correct_negative"
    elif not values:
        outcome = "rejected_positive"
    else:
        outcome = "accepted_correct" if values == [expected] else "accepted_wrong"
    counts[outcome] += 1
    print(f"{image.name}: {outcome}; payloads={values}")
print(json.dumps(counts, sort_keys=True))
if counts["accepted_wrong"] or counts["rejected_positive"] or counts["false_positive"]:
    sys.exit(1)
print("PASS: known-answer public CLI smoke check")
