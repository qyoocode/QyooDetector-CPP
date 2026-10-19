#!/usr/bin/env python3
"""Block release if a known generated symbol is confidently decoded to a wrong ID."""
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

def safe_rejection(run, values):
    if run.returncode != 0 or values:
        return False
    diagnostics = [json.loads(line.split(" = ", 1)[1])
                   for line in run.stdout.splitlines()
                   if line.startswith("Diagnostics JSON = ")]
    if any(record.get("image_loaded") is False or record.get("failure_stage")
           for record in diagnostics):
        return False
    return any(record.get("image_loaded") is True for record in diagnostics)


if "--self-test" in sys.argv:
    def result(code, stderr="", diagnostic=None):
        stdout = "Diagnostics JSON = " + json.dumps(diagnostic) if diagnostic is not None else ""
        return subprocess.CompletedProcess([], code, stdout, stderr)
    assert safe_rejection(result(0, diagnostic={"image_loaded": True}), [])
    # main.cpp emits this exact exit/message for DetectorCoreProcessingFailure,
    # including the failed-allocation branch in DetectorCore.cpp.
    allocation_failure = result(1, "No Qyoo found in the image.")
    assert not safe_rejection(allocation_failure, []), "allocation failure accepted as safe rejection"
    assert not safe_rejection(result(1, "No Qyoo found in the image.", {"image_loaded": True}), [])
    assert not safe_rejection(result(0), [])
    assert not safe_rejection(result(1, "No Qyoo found in the image.", {"image_loaded": False}), [])
    assert not safe_rejection(result(1, "Unable to load image"), [])
    assert not safe_rejection(result(2, "No Qyoo found in the image."), [])
    assert not safe_rejection(result(0, diagnostic={"image_loaded": True}), [0])
    print("PASS: rejection and processing-error classification")
    sys.exit(0)


root = Path(__file__).resolve().parents[1]
folder = root / "tests/fixtures/release-safety"
manifest = json.loads((folder / "manifest.json").read_text())
wrong = rejected = correct = 0
for fixture in manifest["fixtures"]:
    image = folder / fixture["file"]
    if hashlib.sha256(image.read_bytes()).hexdigest() != fixture["sha256"]:
        raise SystemExit(f"FAIL: fixture digest differs: {image.name}")
    run = subprocess.run([str(root / "bin/qyoo_detector"), str(image), "--diagnostics"],
                         cwd=root, capture_output=True, text=True, timeout=30)
    values = [int(value) for value in re.findall(r"^Qyoo value = (\d+)$", run.stdout, re.M)]
    expected = fixture["expected_payload"]
    if not values and not safe_rejection(run, values):
        raise SystemExit(f"FAIL: detector processing error (exit {run.returncode})")
    if not values:
        rejected += 1
        print(f"{image.name}: safe rejection")
    elif run.returncode:
        raise SystemExit(f"FAIL: detector processing error after output (exit {run.returncode})")
    elif all(value == expected for value in values):
        correct += 1
        print(f"{image.name}: accepted correct {expected}")
    else:
        wrong += 1
        print(f"FAIL: {image.name}: accepted wrong; expected {expected}; actual {values}")
print(json.dumps({"accepted_correct": correct, "accepted_wrong": wrong,
                  "rejected": rejected}, sort_keys=True))
sys.exit(1 if wrong else 0)
