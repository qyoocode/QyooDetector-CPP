#!/usr/bin/env python3
"""Qualify explicit known-answer images; no image-generation dependency required.

Manifest: {"fixtures": [{"file": "relative.png", "expected_payload": 1,
"sha256": "...", "group": "optional provenance group"}]}. Null payloads
are negative controls. Rejections are permitted; wrong acceptances and processing
errors fail. Use a dedicated work directory for the CLI's output images.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import subprocess
import time

OUTCOMES = ("accepted_correct", "accepted_wrong", "rejected", "correct_negative", "processing_error")


def counts(cases):
    result = Counter({key: 0 for key in OUTCOMES})
    result.update(case["outcome"] for case in cases)
    return dict(result)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--detector", type=Path,
                        default=Path(__file__).resolve().parents[1] / "bin/qyoo_detector")
    parser.add_argument("--work-dir", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    manifest = args.manifest.resolve()
    detector = args.detector.resolve()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    (args.work_dir / "output").mkdir(exist_ok=True)
    report = {"manifest": str(manifest), "detector": str(detector),
              "detector_sha256": hashlib.sha256(detector.read_bytes()).hexdigest(),
              "counts": {}, "groups": {}, "cases": [], "complete": False,
              "timing_note": "Sequential local CLI wall time; includes process startup and image output."}
    started = time.perf_counter()
    for fixture in json.loads(manifest.read_text())["fixtures"]:
        record = dict(fixture)
        image = (manifest.parent / fixture["file"]).resolve()
        case_started = time.perf_counter()
        try:
            digest = hashlib.sha256(image.read_bytes()).hexdigest()
            if digest != fixture["sha256"]:
                raise ValueError("fixture SHA-256 mismatch")
            run = subprocess.run([str(detector), str(image), "--diagnostics"],
                                 cwd=args.work_dir, capture_output=True, text=True, timeout=30)
            values = [int(value) for value in re.findall(r"^Qyoo value = (\d+)$", run.stdout, re.M)]
            diagnostics = [json.loads(line.split(" = ", 1)[1]) for line in run.stdout.splitlines()
                           if line.startswith("Diagnostics JSON = ")]
            record.update(returncode=run.returncode, values=values, stderr=run.stderr)
            if run.returncode or not diagnostics or any(d.get("image_loaded") is not True or d.get("failure_stage") for d in diagnostics):
                outcome = "processing_error"
                record["stdout"] = run.stdout
            elif values:
                outcome = "accepted_correct" if fixture["expected_payload"] is not None and all(v == fixture["expected_payload"] for v in values) else "accepted_wrong"
                if outcome == "accepted_wrong":
                    record["diagnostics"] = diagnostics
            else:
                outcome = "correct_negative" if fixture["expected_payload"] is None else "rejected"
        except (OSError, ValueError, subprocess.TimeoutExpired) as error:
            outcome = "processing_error"
            record["error"] = f"{type(error).__name__}: {error}"
        record.update(outcome=outcome, wall_seconds=time.perf_counter() - case_started)
        report["cases"].append(record)
        report["counts"] = counts(report["cases"])
        report["groups"] = {group: counts(c for c in report["cases"] if c.get("group", "unspecified") == group) for group in sorted({c.get("group", "unspecified") for c in report["cases"]})}
        report["elapsed_seconds"] = time.perf_counter() - started
        args.report.write_text(json.dumps(report, indent=2) + "\n")
        print(f"{len(report['cases'])}: {fixture['file']}: {outcome}", flush=True)
    report["complete"] = True
    args.report.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report["counts"], sort_keys=True))
    return int(bool(report["counts"].get("accepted_wrong") or report["counts"].get("processing_error")))


if __name__ == "__main__":
    raise SystemExit(main())
