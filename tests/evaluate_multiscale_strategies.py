#!/usr/bin/env python3
"""Evaluate bounded localization-scale strategies with an unchanged detector.

This is a pre-implementation experiment: Pillow performs deterministic LANCZOS
resampling, then the existing detector runs once per unique image/scale. Policy
results are derived from those immutable observations with production-realistic
early exit on the first accepted payload.
"""

from __future__ import annotations

import argparse
import concurrent.futures
import hashlib
import json
import re
import subprocess
import time
from collections import defaultdict
from pathlib import Path
from typing import Any

from PIL import Image, __version__ as PILLOW_VERSION


WORKSPACE = Path(__file__).resolve().parents[2]
DEFAULT_MANIFEST = WORKSPACE / "recovery/task-09/generated-multiscale-matrix/manifest.json"
DEFAULT_DETECTOR = WORKSPACE / "QyooDetector-CPP/bin/qyoo_detector"
DEFAULT_OUTPUT = WORKSPACE / "recovery/task-09/preimplementation-scale-evaluation"
DEFAULT_CACHE = Path("/private/tmp/qyoo-task09-pillow-scale-cache")
BITS_PATTERN = re.compile(r"^(?:Projective )?Binary = ([01]{36})$", re.MULTILINE)
DIAGNOSTIC_PREFIX = "Diagnostics JSON = "


def read_json(path: Path) -> Any:
    return json.loads(path.read_text(encoding="utf-8"))


def write_json(path: Path, value: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def scale_key(scale: float) -> str:
    return f"{scale:.8f}".rstrip("0").rstrip(".").replace(".", "p")


def dimension_scale(width: int, height: int, target_long_edge: int) -> float:
    return min(1.0, target_long_edge / max(width, height))


def unique_scales(case: dict[str, Any]) -> list[float]:
    width, height = case["image"]["dimensions"]
    values = [1.0, 0.40, 0.33,
              dimension_scale(width, height, 1600),
              dimension_scale(width, height, 1320)]
    result: list[float] = []
    for value in values:
        if not any(abs(value - existing) < 1e-9 for existing in result):
            result.append(value)
    return result


def strategies(case: dict[str, Any]) -> dict[str, list[float]]:
    width, height = case["image"]["dimensions"]
    cap1600 = dimension_scale(width, height, 1600)
    cap1320 = dimension_scale(width, height, 1320)
    bounded = []
    for scale in (cap1600, cap1320, 1.0):
        if not any(abs(scale - existing) < 1e-9 for existing in bounded):
            bounded.append(scale)
    return {
        "native_only": [1.0],
        "fixed_40": [0.40],
        "fixed_33": [0.33],
        "fixed_40_then_33": [0.40, 0.33],
        "fixed_33_then_40": [0.33, 0.40],
        "long_edge_1600": [cap1600],
        "bounded_1600_1320_native": bounded,
    }


def parse_diagnostics(output: str) -> dict[str, Any]:
    for line in output.splitlines():
        if line.startswith(DIAGNOSTIC_PREFIX):
            return json.loads(line[len(DIAGNOSTIC_PREFIX):])
    raise ValueError("detector did not emit diagnostics JSON")


def scaled_input(case: dict[str, Any], scale: float, cache: Path) -> tuple[Path, int, int]:
    source = WORKSPACE / case["image"]["path"]
    source_width, source_height = case["image"]["dimensions"]
    if abs(scale - 1.0) < 1e-9:
        return source, source_width, source_height
    width = int(source_width * scale + 0.5)
    height = int(source_height * scale + 0.5)
    destination = cache / scale_key(scale) / f"{case['case_id']}.jpg"
    if not destination.exists():
        destination.parent.mkdir(parents=True, exist_ok=True)
        with Image.open(source) as loaded:
            rgb = loaded.convert("RGB")
            resized = rgb.resize((width, height), Image.Resampling.LANCZOS, reducing_gap=None)
            resized.save(destination, format="JPEG", quality=100, subsampling=0,
                         optimize=False, progressive=False)
    return destination, width, height


def run_observation(case: dict[str, Any], scale: float, detector: Path,
                    cache: Path, timeout: float) -> dict[str, Any]:
    image, width, height = scaled_input(case, scale, cache)
    command = [str(detector), str(image), "--normalization", "carrier-template",
               "--fallback-policy", "qualified", "--diagnostics"]
    started = time.perf_counter()
    try:
        process = subprocess.run(command, cwd=WORKSPACE, capture_output=True, text=True,
                                 timeout=timeout, check=False)
    except subprocess.TimeoutExpired:
        return {"case_id": case["case_id"], "scale": scale, "scale_key": scale_key(scale),
                "image_width": width, "image_height": height, "runtime_ms": timeout * 1000,
                "timeout": True, "error": True, "observed_bits": []}
    runtime_ms = (time.perf_counter() - started) * 1000
    output = process.stdout + process.stderr
    if process.returncode != 0:
        return {"case_id": case["case_id"], "scale": scale, "scale_key": scale_key(scale),
                "image_width": width, "image_height": height, "runtime_ms": runtime_ms,
                "timeout": False, "error": True, "exit_code": process.returncode,
                "error_tail": output[-2000:], "observed_bits": []}
    diagnostic = parse_diagnostics(output)
    stages = diagnostic["stages"]
    observed = BITS_PATTERN.findall(output)
    return {
        "case_id": case["case_id"],
        "scale": scale,
        "scale_key": scale_key(scale),
        "image_width": width,
        "image_height": height,
        "runtime_ms": runtime_ms,
        "timeout": False,
        "error": False,
        "exit_code": process.returncode,
        "observed_bits": observed,
        "raw_feature_count": stages["raw_feature_count"],
        "accepted_candidate_count": stages["accepted_candidate_count"],
        "payload_extracted_count": stages["payload_extracted_count"],
        "normalization_available_count": stages["normalization_available_count"],
        "outline_validation_pass_count": stages["outline_validation_pass_count"],
        "rejection_reason_counts": diagnostic["rejection_reason_counts"],
    }


def derive_policy(case: dict[str, Any], name: str, scales: list[float],
                  lookup: dict[tuple[str, str], dict[str, Any]]) -> dict[str, Any]:
    attempts = []
    for scale in scales:
        observation = lookup[(case["case_id"], scale_key(scale))]
        attempts.append(observation)
        # Production cannot know expected bits. Any safely accepted payload is
        # terminal; whether it was correct is evaluated afterward.
        if observation.get("observed_bits"):
            break
    observed = [bits for attempt in attempts for bits in attempt.get("observed_bits", [])]
    expected = case["expected"]["raw_payload_bits"]
    wrong = [bits for bits in observed if bits != expected]
    exact = bool(observed) and not wrong and all(bits == expected for bits in observed)
    return {
        "case_id": case["case_id"],
        "suite": case["suite"],
        "geometry_profile": case["geometry_profile"],
        "payload_density": case["payload_density"],
        "requested_carrier_size": case["render"]["requested_carrier_size"],
        "approx_carrier_min_dimension": min(case["render"]["approx_carrier_width"],
                                             case["render"]["approx_carrier_height"]),
        "strategy": name,
        "planned_scales": scales,
        "attempted_scales": [attempt["scale"] for attempt in attempts],
        "successful_scale": attempts[-1]["scale"] if observed else None,
        "observed_bits": observed,
        "exact": exact,
        "accepted_wrong": bool(wrong),
        "rejected": not observed,
        "error": any(attempt.get("error", False) for attempt in attempts),
        "runtime_ms": sum(attempt["runtime_ms"] for attempt in attempts),
        "raw_feature_count": sum(attempt.get("raw_feature_count", 0) for attempt in attempts),
        "candidate_count": sum(attempt.get("accepted_candidate_count", 0) for attempt in attempts),
        "attempt_count": len(attempts),
    }


def summarize(results: list[dict[str, Any]]) -> list[dict[str, Any]]:
    grouped: dict[str, list[dict[str, Any]]] = defaultdict(list)
    for result in results:
        grouped[result["strategy"]].append(result)
    output = []
    for strategy, values in sorted(grouped.items()):
        output.append({
            "strategy": strategy,
            "cases": len(values),
            "exact": sum(value["exact"] for value in values),
            "accepted_wrong": sum(value["accepted_wrong"] for value in values),
            "rejected": sum(value["rejected"] for value in values),
            "errors": sum(value["error"] for value in values),
            "mean_runtime_ms": sum(value["runtime_ms"] for value in values) / len(values),
            "mean_raw_features": sum(value["raw_feature_count"] for value in values) / len(values),
            "mean_attempts": sum(value["attempt_count"] for value in values) / len(values),
        })
    return output


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument("--detector", type=Path, default=DEFAULT_DETECTOR)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--cache", type=Path, default=DEFAULT_CACHE)
    parser.add_argument("--workers", type=int, default=4)
    parser.add_argument("--timeout", type=float, default=120.0)
    args = parser.parse_args()
    output = args.output.resolve()
    observations_dir = output / "observations"
    observations_dir.mkdir(parents=True, exist_ok=True)
    manifest = read_json(args.manifest.resolve())
    detector = args.detector.resolve()
    tasks = []
    observations: list[dict[str, Any]] = []
    for case in manifest["cases"]:
        for scale in unique_scales(case):
            result_path = observations_dir / scale_key(scale) / f"{case['case_id']}.json"
            if result_path.exists():
                observations.append(read_json(result_path))
            else:
                tasks.append((case, scale, result_path))
    print(f"existing observations={len(observations)} pending={len(tasks)}", flush=True)

    def execute(task):
        case, scale, result_path = task
        result = run_observation(case, scale, detector, args.cache.resolve(), args.timeout)
        write_json(result_path, result)
        return result

    with concurrent.futures.ThreadPoolExecutor(max_workers=args.workers) as executor:
        futures = [executor.submit(execute, task) for task in tasks]
        for index, future in enumerate(concurrent.futures.as_completed(futures), start=1):
            observations.append(future.result())
            if index % 50 == 0 or index == len(futures):
                print(f"completed {index}/{len(futures)} new observations", flush=True)
    observations.sort(key=lambda value: (value["case_id"], value["scale"]))
    lookup = {(value["case_id"], value["scale_key"]): value for value in observations}
    policy_results = []
    for case in manifest["cases"]:
        for name, scales in strategies(case).items():
            policy_results.append(derive_policy(case, name, scales, lookup))
    document = {
        "schema": "org.qyoo.detector.preimplementation-multiscale-evaluation",
        "schema_version": 1,
        "manifest": str(args.manifest.resolve().relative_to(WORKSPACE)),
        "manifest_sha256": sha256(args.manifest.resolve()),
        "detector": str(detector.relative_to(WORKSPACE)),
        "detector_sha256": sha256(detector),
        "resampler": {"implementation": "Pillow", "version": PILLOW_VERSION,
                      "filter": "Image.Resampling.LANCZOS", "derivative": "JPEG quality 100 4:4:4"},
        "observation_count": len(observations),
        "summary": summarize(policy_results),
        "results": policy_results,
    }
    write_json(output / "results.json", document)
    print(json.dumps(document["summary"], indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
