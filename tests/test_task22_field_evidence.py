#!/usr/bin/env python3
"""Screenshot regression; passing does not clear the unresolved raw-frame incident."""
import hashlib
import json
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FIXTURES = ROOT / "tests/fixtures/task22_field_evidence"
report = json.loads((FIXTURES / "replay.json").read_text())
for case in report["cases"]:
    path = FIXTURES / case["path"]
    assert hashlib.sha256(path.read_bytes()).hexdigest() == case["sha256"], path
    result = subprocess.run([str(ROOT / "bin/qyoo_detector"), str(path)], cwd=ROOT,
                            capture_output=True, text=True, timeout=120, check=True)
    payloads = re.findall(r"^Binary = ([01]{36})$", result.stdout, re.MULTILINE)
    if case["classification"] == "possible-wrong-accept":
        assert not payloads, f"Screenshot accepted unexpected payloads: {payloads}"
    else:
        assert "0" * 36 not in payloads, "Acquisition screenshot accepted all-zero background"
    print(f"{case['classification']}: accepted={payloads}")
print("PASS screenshot regression only; raw-camera-frame field incident remains OPEN")
