#!/usr/bin/env python3
"""Run a manifest through native or production localization with scale traces."""

from __future__ import annotations

import argparse
import concurrent.futures
import hashlib
import json
import re
import subprocess
import time
from collections import Counter
from pathlib import Path
from typing import Any


WORKSPACE = Path(__file__).resolve().parents[2]
DEFAULT_DETECTOR = WORKSPACE / "QyooDetector-CPP/bin/qyoo_detector"
PREFIX = "Diagnostics JSON = "
BITS = re.compile(r"^Binary = ([01]{36})$", re.MULTILINE)


def read_json(path: Path) -> Any:
    return json.loads(path.read_text(encoding="utf-8"))


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def diagnostic(output: str) -> dict[str, Any]:
    lines = [line[len(PREFIX):] for line in output.splitlines()
             if line.startswith(PREFIX)]
    if len(lines) != 1:
        raise RuntimeError(f"expected one diagnostic record, found {len(lines)}")
    return json.loads(lines[0])


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--detector", type=Path, default=DEFAULT_DETECTOR)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--localization-policy", choices=("production", "native"),
                        default="production")
    parser.add_argument("--workers", type=int, default=4)
    parser.add_argument("--timeout", type=float, default=180.0)
    parser.add_argument("--include-accepted-features", action="store_true",
                        help="preserve accepted feature/template diagnostics")
    args = parser.parse_args()
    destination = args.output.resolve()
    if destination.exists():
        raise FileExistsError(f"refusing to overwrite {destination}")
    destination.parent.mkdir(parents=True, exist_ok=True)
    manifest_path = args.manifest.resolve()
    manifest = read_json(manifest_path)
    detector = args.detector.resolve()

    def execute(case: dict[str, Any]) -> dict[str, Any]:
        image = Path(case["image"]["path"])
        if not image.is_absolute():
            image = WORKSPACE / image
        command = [str(detector), str(image), "--diagnostics",
                   "--localization-policy", args.localization_policy]
        started = time.perf_counter()
        try:
            process = subprocess.run(command, cwd=detector.parent.parent,
                                     capture_output=True, text=True, check=False,
                                     timeout=args.timeout)
            combined = process.stdout + process.stderr
            trace = diagnostic(combined) if process.returncode == 0 else None
            error = process.returncode != 0
            exception = None
        except (OSError, subprocess.TimeoutExpired) as caught:
            process = None
            combined = ""
            trace = None
            error = True
            exception = f"{type(caught).__name__}: {caught}"
        observed = BITS.findall(combined)
        expected = case["expected"].get("raw_payload_bits")
        positive = bool(case["expected"]["shape_present"])
        exact = positive and expected is not None and bool(observed) and all(
            bits == expected for bits in observed)
        accepted_wrong = positive and expected is not None and bool(observed) and not exact
        false_payload = not positive and bool(observed)
        if error:
            classification = "error"
        elif exact:
            classification = "exact"
        elif accepted_wrong:
            classification = "accepted_wrong"
        elif false_payload:
            classification = "false_payload"
        elif positive:
            classification = "rejected"
        else:
            classification = "negative_rejected"
        accepted_features = None
        if args.include_accepted_features and trace:
            accepted_features = [
                {
                    "feature_index": feature["feature_index"],
                    "bounds": feature["bounds"],
                    "model_close_fraction": feature["model_close_fraction"],
                    "carrier_projective_rms_pixels": feature["carrier_projective_rms_pixels"],
                    "carrier_projective_max_error_pixels":
                        feature["carrier_projective_max_error_pixels"],
                    "carrier_template": feature["carrier_template_diagnostics"],
                }
                for feature in trace["features"]
                if feature["accepted"] and feature["payload_extracted"]
            ]
        return {
            "case_id": case["case_id"],
            "suite": case.get("suite"),
            "category": case.get("category"),
            "geometry_profile": case.get("geometry_profile"),
            "expected_bits": expected,
            "observed_bits": observed,
            "classification": classification,
            "runtime_ms": (time.perf_counter() - started) * 1000.0,
            "error": error,
            "exception": exception,
            "multiscale": trace.get("multiscale") if trace else None,
            "stages": trace.get("stages") if trace else None,
            "accepted_features": accepted_features,
        }

    results = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.workers) as executor:
        for index, result in enumerate(executor.map(execute, manifest["cases"]), start=1):
            results.append(result)
            if index % 50 == 0:
                print(f"completed {index}/{len(manifest['cases'])}", flush=True)
    counts = Counter(result["classification"] for result in results)
    runtimes = [result["runtime_ms"] for result in results]
    feature_counts = [
        sum(attempt["raw_feature_count"] for attempt in result["multiscale"]["attempts"])
        for result in results if result["multiscale"]
    ]
    output = {
        "schema": "org.qyoo.detector.multiscale-scoreboard",
        "schema_version": 1,
        "manifest": str(manifest_path.relative_to(WORKSPACE)),
        "manifest_sha256": sha256(manifest_path),
        "detector_sha256": sha256(detector),
        "localization_policy": args.localization_policy,
        "summary": {
            "cases": len(results),
            **dict(sorted(counts.items())),
            "mean_runtime_ms": sum(runtimes) / len(runtimes),
            "mean_total_raw_features": sum(feature_counts) / len(feature_counts),
            "mean_attempts": sum(result["multiscale"]["attempted_scale_count"]
                                 for result in results if result["multiscale"]) / len(results),
        },
        "results": results,
    }
    destination.write_text(json.dumps(output, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(output["summary"], indent=2))
    return 1 if counts.get("error") else 0


if __name__ == "__main__":
    raise SystemExit(main())
