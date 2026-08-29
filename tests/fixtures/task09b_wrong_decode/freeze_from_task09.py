#!/usr/bin/env python3
"""Freeze Task 09B wrong-decode evidence into repository fixtures.

This is a one-way evidence importer.  The copied JPEGs are the authoritative
inputs; this helper verifies their hashes and records the original matrix and
detector trace metadata so later generator changes cannot erase the cases.
"""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
from typing import Any


FIXTURE_ROOT = Path(__file__).resolve().parent
WORKSPACE = Path(__file__).resolve().parents[4]
MATRIX_MANIFEST = WORKSPACE / "recovery/task-09/generated-multiscale-matrix/manifest.json"
MATRIX_RESULTS = WORKSPACE / "recovery/task-09/generated-multiscale-production-internal-v2.json"
VISUAL_ROOT = WORKSPACE / "recovery/task-09/wrong-case-visuals-native/cases"

CASE_IDS = (
    "stress_historical_svg_204_02_0160",
    "stress_c_like_512_00_0800",
    "stress_c_like_512_24_0128",
    "stress_c_like_512_24_0400",
    "perspective_historical_svg_204_0220_xy_strong_01",
    "perspective_php_like_512_0360_x_strong_01",
    "perspective_php_like_512_0360_xy_mild_00",
)


def read_json(path: Path) -> Any:
    return json.loads(path.read_text(encoding="utf-8"))


def write_json(path: Path, value: Any) -> None:
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def differing_indexes(expected: str, observed: str) -> list[int]:
    return [index for index, (left, right) in enumerate(zip(expected, observed))
            if left != right]


def compact_feature(feature: dict[str, Any]) -> dict[str, Any]:
    normalizations = feature["visual_geometry"]["normalizations"]
    normalization = next(item for item in normalizations
                         if item["strategy"] == "carrier_template_projective")
    return {
        "feature_index": feature["feature_index"],
        "bounds": feature["bounds"],
        "model_close_fraction": feature["model_close_fraction"],
        "outline": {
            "rms_pixels": feature["projective_outline_rms_pixels"],
            "max_error_pixels": feature["projective_outline_max_error_pixels"],
        },
        "carrier_projective_class": {
            "circle_point_count": feature["carrier_circle_point_count"],
            "first_edge_point_count": feature["carrier_first_edge_point_count"],
            "second_edge_point_count": feature["carrier_second_edge_point_count"],
            "rms_pixels": feature["carrier_projective_rms_pixels"],
            "max_error_pixels": feature["carrier_projective_max_error_pixels"],
        },
        "normalization_strategy": normalization["strategy"],
        "normalized_patch_to_input": normalization["normalized_patch_to_input"],
        "carrier_template": normalization["carrier_template"],
        "patch_size": normalization["patch_size"],
        "background_sample": normalization["background_sample"],
        "sample_region": normalization["sample_region"],
        "cells": normalization["cells"],
    }


def main() -> int:
    matrix = read_json(MATRIX_MANIFEST)
    results = read_json(MATRIX_RESULTS)
    cases_by_id = {case["case_id"]: case for case in matrix["cases"]}
    results_by_id = {case["case_id"]: case for case in results["results"]}
    frozen_cases = []
    for case_id in CASE_IDS:
        source_case = cases_by_id[case_id]
        source_result = results_by_id[case_id]
        expected = source_case["expected"]["raw_payload_bits"]
        observed_values = source_result["observed_bits"]
        if source_result["classification"] != "accepted_wrong" or len(observed_values) != 1:
            raise AssertionError(f"{case_id}: source result is no longer one accepted wrong decode")
        observed = observed_values[0]

        fixture_image = FIXTURE_ROOT / "images" / f"{case_id}.jpg"
        fixture_hash = sha256(fixture_image)
        if fixture_hash != source_case["image"]["sha256"]:
            raise AssertionError(f"{case_id}: fixture image hash differs from matrix evidence")

        trace_path = VISUAL_ROOT / case_id / "detector-trace.json"
        trace = read_json(trace_path)
        features = [feature for feature in trace["features"]
                    if feature["accepted"] and feature["payload_extracted"]]
        if len(features) != 1:
            raise AssertionError(f"{case_id}: expected one accepted trace feature")
        compact_trace = {
            "schema": "org.qyoo.detector.task09b-reference-trace",
            "schema_version": 1,
            "case_id": case_id,
            "image_size": trace["image_size"],
            "stages": trace["stages"],
            "accepted_feature": compact_feature(features[0]),
        }
        trace_fixture = FIXTURE_ROOT / "reference-traces" / f"{case_id}.json"
        write_json(trace_fixture, compact_trace)

        frozen_cases.append({
            "case_id": case_id,
            "provenance": {
                "task09_matrix_manifest_sha256": sha256(MATRIX_MANIFEST),
                "task09_matrix_result_sha256": sha256(MATRIX_RESULTS),
                "task09_visual_trace_sha256": sha256(trace_path),
                "task09_experimental_detector_sha256": results["detector_sha256"],
            },
            "image": {
                "path": f"images/{case_id}.jpg",
                "sha256": fixture_hash,
                "format": source_case["image"]["format"],
                "dimensions": source_case["image"]["dimensions"],
            },
            "expected_bits": expected,
            "observed_reference_bits": observed,
            "differing_bit_indexes_msb_first": differing_indexes(expected, observed),
            "geometry_profile": source_case["geometry_profile"],
            "payload_density": source_case["payload_density"],
            "render": source_case["render"],
            "localization_path": "native",
            "normalization_strategy": "carrier_template_projective",
            "reference_trace": f"reference-traces/{case_id}.json",
        })

    manifest = {
        "schema": "org.qyoo.detector.task09b-wrong-decode-fixtures",
        "schema_version": 1,
        "purpose": "Immutable JPEG regression inputs for the seven Task 09 native-path accepted-wrong decodes.",
        "bit_index_convention": "MSB-first textual index 0 through 35",
        "case_count": len(frozen_cases),
        "cases": frozen_cases,
    }
    write_json(FIXTURE_ROOT / "manifest.json", manifest)
    print(f"froze {len(frozen_cases)} cases")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
