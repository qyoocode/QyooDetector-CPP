#!/usr/bin/env python3
"""Measure independent raster-scale confirmation for selected detector cases."""

from __future__ import annotations

import argparse
import concurrent.futures
import json
from collections import defaultdict
from pathlib import Path
from typing import Any

from evaluate_multiscale_strategies import (
    DEFAULT_CACHE,
    DEFAULT_DETECTOR,
    WORKSPACE,
    read_json,
    run_observation,
    scale_key,
    sha256,
    write_json,
)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--case-id", action="append", dest="case_ids",
                        help="limit evaluation to one or more case IDs")
    parser.add_argument("--scales", required=True,
                        help="comma-separated input scales, for example 0.9,0.8,0.5")
    parser.add_argument("--detector", type=Path, default=DEFAULT_DETECTOR)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--cache", type=Path, default=DEFAULT_CACHE)
    parser.add_argument("--workers", type=int, default=4)
    parser.add_argument("--timeout", type=float, default=120.0)
    args = parser.parse_args()

    scales = [float(value) for value in args.scales.split(",")]
    if not scales or any(value <= 0.0 or value > 1.0 for value in scales):
        parser.error("all scales must be in (0, 1]")

    manifest_path = args.manifest.resolve()
    manifest = read_json(manifest_path)
    cases_to_run = manifest["cases"]
    if args.case_ids:
        requested = set(args.case_ids)
        cases_to_run = [case for case in cases_to_run if case["case_id"] in requested]
        missing = requested - {case["case_id"] for case in cases_to_run}
        if missing:
            parser.error("unknown case IDs: " + ", ".join(sorted(missing)))
    detector = args.detector.resolve()
    output = args.output.resolve()
    observations_dir = output / "observations"
    tasks: list[tuple[dict[str, Any], float, Path]] = []
    observations: list[dict[str, Any]] = []
    for case in cases_to_run:
        for scale in scales:
            destination = observations_dir / scale_key(scale) / f"{case['case_id']}.json"
            if destination.exists():
                observations.append(read_json(destination))
            else:
                tasks.append((case, scale, destination))

    print(f"existing observations={len(observations)} pending={len(tasks)}", flush=True)

    def execute(task: tuple[dict[str, Any], float, Path]) -> dict[str, Any]:
        case, scale, destination = task
        result = run_observation(case, scale, detector, args.cache.resolve(), args.timeout)
        write_json(destination, result)
        return result

    with concurrent.futures.ThreadPoolExecutor(max_workers=args.workers) as executor:
        futures = [executor.submit(execute, task) for task in tasks]
        for index, future in enumerate(concurrent.futures.as_completed(futures), start=1):
            observations.append(future.result())
            if index % 50 == 0 or index == len(futures):
                print(f"completed {index}/{len(futures)}", flush=True)

    cases = {case["case_id"]: case for case in cases_to_run}
    grouped: dict[str, list[dict[str, Any]]] = defaultdict(list)
    results = []
    for observation in sorted(observations, key=lambda value: (value["scale"], value["case_id"])):
        case = cases[observation["case_id"]]
        expected = case["expected"]["raw_payload_bits"]
        observed = observation.get("observed_bits", [])
        classification = (
            "exact" if observed and all(value == expected for value in observed)
            else "wrong" if observed
            else "rejected"
        )
        result = {**observation, "expected_bits": expected, "classification": classification}
        results.append(result)
        grouped[observation["scale_key"]].append(result)

    summary = []
    for key, values in sorted(grouped.items(), key=lambda item: item[1][0]["scale"]):
        summary.append({
            "scale": values[0]["scale"],
            "scale_key": key,
            "cases": len(values),
            "exact": sum(value["classification"] == "exact" for value in values),
            "accepted_wrong": sum(value["classification"] == "wrong" for value in values),
            "rejected": sum(value["classification"] == "rejected" for value in values),
            "errors": sum(bool(value.get("error")) for value in values),
        })
    document = {
        "schema": "org.qyoo.detector.confirmation-scale-evaluation",
        "schema_version": 1,
        "manifest": str(manifest_path.relative_to(WORKSPACE)),
        "manifest_sha256": sha256(manifest_path),
        "detector_sha256": sha256(detector),
        "scales": scales,
        "summary": summary,
        "results": results,
    }
    write_json(output / "results.json", document)
    print(json.dumps(summary, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
