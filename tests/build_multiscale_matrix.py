#!/usr/bin/env python3
"""Build the deterministic Task 09 multiscale localization matrix.

The images are product-test inputs, not a new Qyoo rendering definition.  They
exercise every evidence-backed Python geometry profile across independently
varied carrier size, payload density, pose, background, and camera effects.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import random
import subprocess
import sys
from pathlib import Path
from typing import Any

import cv2
import numpy as np


WORKSPACE = Path(__file__).resolve().parents[2]
GENERATOR_ROOT = WORKSPACE / "QyooGenerate-Python"
DEFAULT_OUTPUT = WORKSPACE / "recovery/task-09/generated-multiscale-matrix"
SEED = 20260830
PROFILES = ("historical_svg_204", "c_like_512", "php_like_512", "legacy_python")
PAYLOAD_DENSITIES = (0, 1, 2, 4, 8, 12, 18, 24, 30, 36)
STRESS_SIZES = (96, 128, 160, 200, 256, 320, 400, 512, 640, 800, 1000, 1200)
ROTATION_SIZES = (128, 200, 300, 450, 650, 900)
ROTATIONS = tuple(range(0, 360, 15))
PERSPECTIVE_SIZES = (140, 220, 360, 600, 900)
PERSPECTIVES = (
    ("frontal", 0.0, 0.0),
    ("x_mild", 0.18, 0.0),
    ("x_strong", 0.34, 0.0),
    ("y_mild", 0.0, 0.18),
    ("y_strong", 0.0, -0.34),
    ("xy_mild", 0.16, -0.14),
    ("xy_strong", -0.30, 0.28),
)
BACKGROUND_NAMES = (
    "light", "dark", "horizontal_gradient", "checker",
    "diagonal_stripes", "sensor_noise", "geometric_clutter", "textured",
)
QUIET_ZONES = (0.0, 0.025, 0.05, 0.10)
BLUR_SIGMAS = (0.0, 0.6, 1.2, 2.0, 3.2)
NOISE_SIGMAS = (0.0, 1.5, 4.0, 8.0, 14.0)
JPEG_QUALITIES = (100, 92, 80, 65, 50)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def bits_with_density(density: int, variant: int) -> str:
    if density == 0:
        return "0" * 36
    if density == 36:
        return "1" * 36
    if density == 18 and variant % 2 == 0:
        return "01" * 18
    rng = random.Random(SEED + density * 1009 + variant * 9176)
    indexes = set(rng.sample(range(36), density))
    return "".join("1" if index in indexes else "0" for index in range(36))


def canvas_dimensions(carrier_size: int) -> tuple[int, int]:
    if carrier_size <= 400:
        return 1024, 768
    if carrier_size <= 800:
        return 1440, 1080
    return 1920, 1600


def make_background(name: str, width: int, height: int, seed: int) -> np.ndarray:
    rng = np.random.default_rng(seed)
    yy, xx = np.mgrid[0:height, 0:width]
    if name == "light":
        return np.full((height, width, 3), (224, 226, 220), np.uint8)
    if name == "dark":
        return np.full((height, width, 3), (30, 35, 42), np.uint8)
    if name == "horizontal_gradient":
        value = np.clip(42 + xx * 175 / max(1, width - 1), 0, 255).astype(np.uint8)
        return np.dstack((value, np.flip(value, axis=1), np.full_like(value, 132)))
    if name == "checker":
        cells = ((xx // 48 + yy // 48) % 2).astype(np.uint8)
        value = 72 + cells * 120
        return np.dstack((value, value, value)).astype(np.uint8)
    if name == "diagonal_stripes":
        bands = (((xx + 2 * yy) // 19) % 2).astype(np.uint8)
        return np.dstack((55 + bands * 155, 82 + bands * 125,
                          115 + bands * 95)).astype(np.uint8)
    if name == "sensor_noise":
        base = rng.normal(126, 48, (height, width, 3))
        return np.clip(base, 0, 255).astype(np.uint8)
    if name == "geometric_clutter":
        image = np.full((height, width, 3), 186, np.uint8)
        for index in range(90):
            color = tuple(int(value) for value in rng.integers(20, 236, 3))
            center = tuple(int(value) for value in
                           (rng.integers(0, width), rng.integers(0, height)))
            radius = int(rng.integers(7, max(8, min(width, height) // 14)))
            if index % 2:
                cv2.circle(image, center, radius, color, int(rng.integers(1, 6)))
            else:
                cv2.rectangle(image, (max(0, center[0] - radius), max(0, center[1] - radius)),
                              (min(width - 1, center[0] + radius),
                               min(height - 1, center[1] + radius)), color,
                              int(rng.integers(1, 6)))
        return image
    wave = (35 * np.sin(xx / 8.3) + 29 * np.cos(yy / 11.7)
            + 22 * np.sin((xx + yy) / 17.1))
    base = np.clip(128 + wave + rng.normal(0, 18, (height, width)), 0, 255).astype(np.uint8)
    return np.dstack((base, np.roll(base, 13, axis=1), np.roll(base, 19, axis=0)))


def target_rgba(rendered: np.ndarray, carrier_size: int, quiet_fraction: float) -> tuple[np.ndarray, int]:
    alpha = rendered[:, :, 3]
    ys, xs = np.nonzero(alpha)
    crop = rendered[ys.min():ys.max() + 1, xs.min():xs.max() + 1]
    carrier = cv2.resize(crop, (carrier_size, carrier_size), interpolation=cv2.INTER_LANCZOS4)
    quiet = int(round(carrier_size * quiet_fraction))
    if quiet == 0:
        return carrier, quiet
    target = np.full((carrier_size + 2 * quiet, carrier_size + 2 * quiet, 4), 255, np.uint8)
    target[quiet:quiet + carrier_size, quiet:quiet + carrier_size] = carrier
    # White presentation area remains opaque while the carrier pixels retain
    # their exact scaled values. The carrier itself is never redrawn.
    return target, quiet


def destination_quad(target_size: int, canvas_width: int, canvas_height: int,
                     rotation_degrees: float, perspective_x: float,
                     perspective_y: float, offset_x: float, offset_y: float) -> np.ndarray:
    radians = math.radians(rotation_degrees)
    cosine, sine = math.cos(radians), math.sin(radians)
    points = []
    for x, y in ((-0.5, -0.5), (0.5, -0.5), (0.5, 0.5), (-0.5, 0.5)):
        rotated_x = cosine * x - sine * y
        rotated_y = sine * x + cosine * y
        denominator = 1.0 + perspective_x * rotated_x + perspective_y * rotated_y
        points.append((canvas_width * 0.5 + offset_x + target_size * rotated_x / denominator,
                       canvas_height * 0.5 + offset_y + target_size * rotated_y / denominator))
    return np.float32(points)


def compose_case(profile: str, bits: str, carrier_size: int, rotation: float,
                 perspective_x: float, perspective_y: float, background_name: str,
                 quiet_fraction: float, blur_sigma: float, noise_sigma: float,
                 lighting_strength: float, jpeg_quality: int, seed: int,
                 destination: Path) -> dict[str, Any]:
    random.seed(seed)
    np.random.seed(seed & 0xFFFFFFFF)
    sys.path.insert(0, str(GENERATOR_ROOT / "src"))
    from generate_qyoo_synthetic import render_qyoo

    pil_image, returned_bits = render_qyoo(bits, geometry_profile=profile)
    if returned_bits != bits:
        raise AssertionError("renderer changed payload")
    rendered = np.asarray(pil_image)
    target, quiet_pixels = target_rgba(rendered, carrier_size, quiet_fraction)
    width, height = canvas_dimensions(carrier_size)
    background = make_background(background_name, width, height, seed + 313)
    offset_x = ((seed % 17) - 8) * min(9.0, width / 180.0)
    offset_y = (((seed // 17) % 17) - 8) * min(7.0, height / 180.0)
    quad = destination_quad(target.shape[1], width, height, rotation,
                            perspective_x, perspective_y, offset_x, offset_y)
    source_quad = np.float32([[0, 0], [target.shape[1] - 1, 0],
                              [target.shape[1] - 1, target.shape[0] - 1],
                              [0, target.shape[0] - 1]])
    matrix = cv2.getPerspectiveTransform(source_quad, quad)
    warped = cv2.warpPerspective(target, matrix, (width, height), flags=cv2.INTER_LINEAR,
                                 borderMode=cv2.BORDER_CONSTANT, borderValue=(0, 0, 0, 0))
    alpha = warped[:, :, 3:4].astype(np.float32) / 255.0
    image = (warped[:, :, :3].astype(np.float32) * alpha
             + background.astype(np.float32) * (1.0 - alpha))
    if lighting_strength:
        yy, xx = np.mgrid[0:height, 0:width]
        direction = math.radians((seed % 360))
        gradient = ((math.cos(direction) * (xx / max(1, width - 1) - 0.5)
                     + math.sin(direction) * (yy / max(1, height - 1) - 0.5)))
        image *= np.clip(1.0 + lighting_strength * gradient[:, :, None], 0.45, 1.55)
    if blur_sigma > 0:
        image = cv2.GaussianBlur(image, (0, 0), blur_sigma)
    if noise_sigma > 0:
        rng = np.random.default_rng(seed + 991)
        image += rng.normal(0, noise_sigma, image.shape)
    image = np.clip(image, 0, 255).astype(np.uint8)
    destination.parent.mkdir(parents=True, exist_ok=True)
    if not cv2.imwrite(str(destination), cv2.cvtColor(image, cv2.COLOR_RGB2BGR),
                       [cv2.IMWRITE_JPEG_QUALITY, jpeg_quality,
                        cv2.IMWRITE_JPEG_PROGRESSIVE, 0,
                        cv2.IMWRITE_JPEG_OPTIMIZE, 0]):
        raise RuntimeError(f"unable to save {destination}")

    inset = quiet_pixels / max(1, target.shape[0] - 1)
    carrier_source = np.float32([
        [inset * (target.shape[1] - 1), inset * (target.shape[0] - 1)],
        [(1 - inset) * (target.shape[1] - 1), inset * (target.shape[0] - 1)],
        [(1 - inset) * (target.shape[1] - 1), (1 - inset) * (target.shape[0] - 1)],
        [inset * (target.shape[1] - 1), (1 - inset) * (target.shape[0] - 1)],
    ]).reshape(-1, 1, 2)
    carrier_quad = cv2.perspectiveTransform(carrier_source, matrix).reshape(-1, 2)
    carrier_widths = [np.linalg.norm(carrier_quad[1] - carrier_quad[0]),
                      np.linalg.norm(carrier_quad[2] - carrier_quad[3])]
    carrier_heights = [np.linalg.norm(carrier_quad[3] - carrier_quad[0]),
                       np.linalg.norm(carrier_quad[2] - carrier_quad[1])]
    return {
        "dimensions": [width, height],
        "source_to_image_homography": matrix.astype(float).tolist(),
        "target_quad": quad.astype(float).tolist(),
        "carrier_quad": carrier_quad.astype(float).tolist(),
        "approx_carrier_width": float(sum(carrier_widths) / 2),
        "approx_carrier_height": float(sum(carrier_heights) / 2),
        "quiet_zone_pixels": quiet_pixels,
    }


def case_specifications() -> list[dict[str, Any]]:
    cases: list[dict[str, Any]] = []
    serial = 0

    # Broad deterministic cross-factor population. Each factor cycles with a
    # relatively-prime stride so the matrix covers combinations without a huge
    # full Cartesian product.
    for profile_index, profile in enumerate(PROFILES):
        for density_index, density in enumerate(PAYLOAD_DENSITIES):
            for size_index, carrier_size in enumerate(STRESS_SIZES):
                serial += 1
                cases.append({
                    "case_id": f"stress_{profile}_{density:02d}_{carrier_size:04d}",
                    "suite": "cross_factor_stress",
                    "profile": profile,
                    "bits": bits_with_density(density, profile_index * 100 + size_index),
                    "carrier_size": carrier_size,
                    "rotation": ROTATIONS[(serial * 7) % len(ROTATIONS)],
                    "perspective_x": (-0.32, -0.18, 0.0, 0.16, 0.30)[serial % 5],
                    "perspective_y": (-0.28, -0.14, 0.0, 0.17, 0.31)[(serial * 3) % 5],
                    "background": BACKGROUND_NAMES[(serial * 5) % len(BACKGROUND_NAMES)],
                    "quiet": QUIET_ZONES[(serial * 3) % len(QUIET_ZONES)],
                    "blur": BLUR_SIGMAS[(serial * 2) % len(BLUR_SIGMAS)],
                    "noise": NOISE_SIGMAS[(serial * 3) % len(NOISE_SIGMAS)],
                    "lighting": (0.0, 0.18, 0.35, 0.55)[serial % 4],
                    "jpeg_quality": JPEG_QUALITIES[(serial * 2) % len(JPEG_QUALITIES)],
                })

    # Controlled angle × carrier-size slice: isolates alias/contour resonance.
    for profile in PROFILES[:3]:
        for carrier_size in ROTATION_SIZES:
            for rotation in ROTATIONS:
                serial += 1
                cases.append({
                    "case_id": f"rotation_{profile}_{carrier_size:04d}_{rotation:03d}",
                    "suite": "rotation_size_control",
                    "profile": profile,
                    "bits": bits_with_density(18, carrier_size + rotation),
                    "carrier_size": carrier_size,
                    "rotation": rotation,
                    "perspective_x": 0.0,
                    "perspective_y": 0.0,
                    "background": "light",
                    "quiet": 0.0,
                    "blur": 0.0,
                    "noise": 0.0,
                    "lighting": 0.0,
                    "jpeg_quality": 100,
                })

    # Controlled X/Y and combined perspective across sparse/dense payloads.
    for profile in PROFILES[:3]:
        for carrier_size in PERSPECTIVE_SIZES:
            for perspective_name, perspective_x, perspective_y in PERSPECTIVES:
                for density in (0, 1, 18, 36):
                    serial += 1
                    cases.append({
                        "case_id": (f"perspective_{profile}_{carrier_size:04d}_"
                                    f"{perspective_name}_{density:02d}"),
                        "suite": "perspective_density_control",
                        "profile": profile,
                        "bits": bits_with_density(density, serial),
                        "carrier_size": carrier_size,
                        "rotation": (0, 35, 90, 145)[density % 4],
                        "perspective_x": perspective_x,
                        "perspective_y": perspective_y,
                        "background": "light",
                        "quiet": 0.05 if density % 2 else 0.0,
                        "blur": 0.6 if density == 1 else 0.0,
                        "noise": 1.5 if density == 18 else 0.0,
                        "lighting": 0.18 if density == 36 else 0.0,
                        "jpeg_quality": 92,
                    })
    return cases


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()
    output = args.output.resolve()
    if output.exists() and any(output.iterdir()):
        raise FileExistsError(f"refusing to overwrite non-empty {output}")
    images = output / "images"
    images.mkdir(parents=True)
    specifications = case_specifications()
    cases = []
    for index, specification in enumerate(specifications):
        image_path = images / f"{specification['case_id']}.jpg"
        seed = SEED + index * 7919
        geometry = compose_case(
            specification["profile"], specification["bits"], specification["carrier_size"],
            specification["rotation"], specification["perspective_x"],
            specification["perspective_y"], specification["background"],
            specification["quiet"], specification["blur"], specification["noise"],
            specification["lighting"], specification["jpeg_quality"], seed, image_path)
        cases.append({
            "case_id": specification["case_id"],
            "suite": specification["suite"],
            "category": specification["suite"],
            "geometry_profile": specification["profile"],
            "payload_density": specification["bits"].count("1"),
            "image": {
                "path": str(image_path.relative_to(WORKSPACE)),
                "sha256": sha256(image_path),
                "format": "JPEG",
                "dimensions": geometry["dimensions"],
            },
            "expected": {
                "shape_present": True,
                "raw_payload_bits": specification["bits"],
                "raw_payload_integer": int(specification["bits"], 2),
                "profile": "recovery_raw36",
            },
            "render": {
                "requested_carrier_size": specification["carrier_size"],
                "rotation_z_degrees": specification["rotation"],
                "perspective_x": specification["perspective_x"],
                "perspective_y": specification["perspective_y"],
                "background": specification["background"],
                "quiet_zone_fraction": specification["quiet"],
                "blur_sigma": specification["blur"],
                "noise_sigma": specification["noise"],
                "lighting_strength": specification["lighting"],
                "jpeg_quality": specification["jpeg_quality"],
                "random_seed": seed,
                **geometry,
            },
        })
        if index % 100 == 0 or index + 1 == len(specifications):
            print(f"generated {index + 1}/{len(specifications)}", flush=True)

    generator_commit = subprocess.run(
        ["git", "rev-parse", "HEAD"], cwd=GENERATOR_ROOT, check=True,
        capture_output=True, text=True).stdout.strip()
    manifest = {
        "schema": "org.qyoo.detector.multiscale-matrix",
        "schema_version": 1,
        "seed": SEED,
        "generator": {"repository": "QyooGenerate-Python", "commit": generator_commit,
                      "geometry_profiles": list(PROFILES)},
        "matrix_design": {
            "stress_sizes": list(STRESS_SIZES),
            "rotation_sizes": list(ROTATION_SIZES),
            "rotations_degrees": list(ROTATIONS),
            "perspective_sizes": list(PERSPECTIVE_SIZES),
            "perspective_conditions": [list(value) for value in PERSPECTIVES],
            "payload_densities": list(PAYLOAD_DENSITIES),
            "backgrounds": list(BACKGROUND_NAMES),
            "quiet_zone_fractions": list(QUIET_ZONES),
            "blur_sigmas": list(BLUR_SIGMAS),
            "noise_sigmas": list(NOISE_SIGMAS),
            "jpeg_qualities": list(JPEG_QUALITIES),
        },
        "case_count": len(cases),
        "cases": cases,
    }
    manifest_path = output / "manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"manifest": str(manifest_path), "cases": len(cases)}, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
