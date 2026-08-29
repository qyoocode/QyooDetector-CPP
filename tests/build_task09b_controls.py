#!/usr/bin/env python3
"""Generate deterministic nearest-neighbor controls for Task 09B failures."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any

from build_multiscale_matrix import bits_with_density, compose_case


WORKSPACE = Path(__file__).resolve().parents[2]
DEFAULT_SOURCE = WORKSPACE / "recovery/task-09/generated-multiscale-matrix/manifest.json"
DEFAULT_OUTPUT = WORKSPACE / "recovery/task-09b/nearest-controls"
WRONG_CASE_IDS = (
    "stress_historical_svg_204_02_0160",
    "stress_c_like_512_00_0800",
    "stress_c_like_512_24_0128",
    "stress_c_like_512_24_0400",
    "perspective_historical_svg_204_0220_xy_strong_01",
    "perspective_php_like_512_0360_x_strong_01",
    "perspective_php_like_512_0360_xy_mild_00",
)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def read_json(path: Path) -> Any:
    return json.loads(path.read_text(encoding="utf-8"))


def write_json(path: Path, value: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


def variants(case: dict[str, Any]) -> list[tuple[str, str, dict[str, Any]]]:
    render = case["render"]
    expected = case["expected"]["raw_payload_bits"]
    dense = bits_with_density(18, render["random_seed"])
    values: list[tuple[str, str, dict[str, Any]]] = [
        ("payload_dense18", dense, {}),
        ("payload_all_ones", "1" * 36, {}),
        ("perspective_75pct", expected,
         {"perspective_x": render["perspective_x"] * 0.75,
          "perspective_y": render["perspective_y"] * 0.75}),
        ("perspective_50pct", expected,
         {"perspective_x": render["perspective_x"] * 0.50,
          "perspective_y": render["perspective_y"] * 0.50}),
        ("perspective_25pct", expected,
         {"perspective_x": render["perspective_x"] * 0.25,
          "perspective_y": render["perspective_y"] * 0.25}),
        ("perspective_none", expected, {"perspective_x": 0.0, "perspective_y": 0.0}),
        ("blur_none", expected, {"blur_sigma": 0.0}),
        ("blur_50pct", expected, {"blur_sigma": render["blur_sigma"] * 0.5}),
        ("noise_none", expected, {"noise_sigma": 0.0}),
        ("lighting_none", expected, {"lighting_strength": 0.0}),
        ("background_light", expected, {"background": "light"}),
        ("jpeg_100", expected, {"jpeg_quality": 100}),
        ("quiet_10pct", expected, {"quiet_zone_fraction": 0.10}),
        ("effects_clean", expected,
         {"blur_sigma": 0.0, "noise_sigma": 0.0,
          "lighting_strength": 0.0, "jpeg_quality": 100}),
        ("camera_clean", expected,
         {"background": "light", "quiet_zone_fraction": 0.05,
          "blur_sigma": 0.0, "noise_sigma": 0.0,
          "lighting_strength": 0.0, "jpeg_quality": 100}),
        ("size_80pct", expected,
         {"requested_carrier_size": max(64, int(round(render["requested_carrier_size"] * 0.8)))}),
        ("size_90pct", expected,
         {"requested_carrier_size": max(64, int(round(render["requested_carrier_size"] * 0.9)))}),
        ("size_110pct", expected,
         {"requested_carrier_size": int(round(render["requested_carrier_size"] * 1.1))}),
        ("size_120pct", expected,
         {"requested_carrier_size": int(round(render["requested_carrier_size"] * 1.2))}),
        ("size_130pct", expected,
         {"requested_carrier_size": int(round(render["requested_carrier_size"] * 1.3))}),
        ("size_150pct", expected,
         {"requested_carrier_size": int(round(render["requested_carrier_size"] * 1.5))}),
    ]
    if render["perspective_x"]:
        values.append(("perspective_x_75pct", expected,
                       {"perspective_x": render["perspective_x"] * 0.75}))
        values.append(("perspective_x_50pct", expected,
                       {"perspective_x": render["perspective_x"] * 0.50}))
    if render["perspective_y"]:
        values.append(("perspective_y_75pct", expected,
                       {"perspective_y": render["perspective_y"] * 0.75}))
        values.append(("perspective_y_50pct", expected,
                       {"perspective_y": render["perspective_y"] * 0.50}))
    return values


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=DEFAULT_SOURCE)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()
    source = read_json(args.source.resolve())
    by_id = {case["case_id"]: case for case in source["cases"]}
    output = args.output.resolve()
    cases = []
    for base_id in WRONG_CASE_IDS:
        base = by_id[base_id]
        original = base["render"]
        for variant_name, bits, overrides in variants(base):
            values = {
                "requested_carrier_size": original["requested_carrier_size"],
                "rotation_z_degrees": original["rotation_z_degrees"],
                "perspective_x": original["perspective_x"],
                "perspective_y": original["perspective_y"],
                "background": original["background"],
                "quiet_zone_fraction": original["quiet_zone_fraction"],
                "blur_sigma": original["blur_sigma"],
                "noise_sigma": original["noise_sigma"],
                "lighting_strength": original["lighting_strength"],
                "jpeg_quality": original["jpeg_quality"],
            }
            values.update(overrides)
            case_id = f"{base_id}__{variant_name}"
            image = output / "images" / f"{case_id}.jpg"
            metadata = compose_case(
                base["geometry_profile"], bits,
                values["requested_carrier_size"], values["rotation_z_degrees"],
                values["perspective_x"], values["perspective_y"], values["background"],
                values["quiet_zone_fraction"], values["blur_sigma"], values["noise_sigma"],
                values["lighting_strength"], values["jpeg_quality"],
                original["random_seed"], image)
            cases.append({
                "case_id": case_id,
                "base_wrong_case_id": base_id,
                "variant": variant_name,
                "changed_fields": sorted(overrides) if overrides else ["payload"],
                "suite": "task09b_nearest_control",
                "category": "task09b_nearest_control",
                "geometry_profile": base["geometry_profile"],
                "payload_density": bits.count("1"),
                "image": {"path": str(image.relative_to(WORKSPACE)), "sha256": sha256(image),
                          "format": "JPEG", "dimensions": metadata["dimensions"]},
                "expected": {"shape_present": True, "raw_payload_bits": bits,
                             "raw_payload_integer": int(bits, 2), "profile": "recovery_raw36"},
                "render": {**values, "random_seed": original["random_seed"], **metadata},
            })
    manifest = {
        "schema": "org.qyoo.detector.task09b-nearest-controls",
        "schema_version": 1,
        "source_manifest": str(args.source.resolve()),
        "case_count": len(cases),
        "cases": cases,
    }
    write_json(output / "manifest.json", manifest)
    print(f"generated {len(cases)} controls in {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
