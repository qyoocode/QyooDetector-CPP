#!/usr/bin/env python3
"""Reproduce the carrier-template ambiguity objective without detector changes.

The tool consumes one detector visual trace and its original image, evaluates
the exact 101-point Task 07 search, and emits the objective decomposition and
payload transitions needed for Task 09B safety analysis.
"""

from __future__ import annotations

import argparse
import json
import math
from pathlib import Path
from typing import Any

import cv2
import numpy as np


PATCH_SIZE = 176
RADIANCE_DISTANCE_MATCH = 60
MINIMUM_AMOUNT = -0.25
MAXIMUM_AMOUNT = 0.25
SEARCH_STEPS = 101
MODEL_LOWER = 0.5 - (0.5 / math.sqrt(2.0))
MODEL_UPPER = 0.5 + (0.5 / math.sqrt(2.0))
DOT_RADIUS = (MODEL_UPPER - MODEL_LOWER) / 12.0
PATCH_MODEL_LOWER = MODEL_LOWER - 2.0 * DOT_RADIUS
PATCH_MODEL_UPPER = MODEL_UPPER + 2.0 * DOT_RADIUS


def read_json(path: Path) -> Any:
    return json.loads(path.read_text(encoding="utf-8"))


def write_json(path: Path, value: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


def carrier_ambiguity_transform(amount: float) -> np.ndarray:
    unit_from_model = np.array(((2.0, 0.0, -1.0),
                                (0.0, 2.0, -1.0),
                                (0.0, 0.0, 1.0)))
    model_from_unit = np.array(((0.5, 0.0, 0.5),
                                (0.0, 0.5, 0.5),
                                (0.0, 0.0, 1.0)))
    fixed_basis = np.array(((0.0, -1.0, -1.0),
                            (-1.0, 0.0, -1.0),
                            (1.0, 1.0, 1.0)))
    inverse_fixed_basis = np.array(((1.0, 0.0, 1.0),
                                    (0.0, 1.0, 1.0),
                                    (-1.0, -1.0, -1.0)))
    boost = np.diag((math.exp(amount), math.exp(-amount), 1.0))
    return model_from_unit @ fixed_basis @ boost @ inverse_fixed_basis @ unit_from_model


def model_dot_location(column: int, row: int) -> tuple[float, float]:
    return (MODEL_LOWER + DOT_RADIUS + 2.0 * DOT_RADIUS * column,
            MODEL_LOWER + DOT_RADIUS + 2.0 * DOT_RADIUS * row)


def sample_patch(red: np.ndarray, normalized_to_input: np.ndarray) -> np.ndarray:
    yy, xx = np.mgrid[0:PATCH_SIZE, 0:PATCH_SIZE]
    normalized = np.stack((xx.ravel() / PATCH_SIZE,
                           yy.ravel() / PATCH_SIZE,
                           np.ones(PATCH_SIZE * PATCH_SIZE)), axis=0)
    source = normalized_to_input @ normalized
    source_x = np.rint(source[0] / source[2]).astype(np.int64)
    source_y = np.rint(source[1] / source[2]).astype(np.int64)
    source_x = np.clip(source_x, 0, red.shape[1] - 1)
    source_y = np.clip(source_y, 0, red.shape[0] - 1)
    patch = red[source_y, source_x].reshape(PATCH_SIZE, PATCH_SIZE).astype(np.float64)
    minimum = int(patch.min())
    maximum = int(patch.max())
    if maximum > minimum:
        scale = 256.0 / (maximum - minimum)
        patch = np.minimum(255, ((patch - minimum) * scale).astype(np.int64))
    return patch.astype(np.uint8)


def evaluate_patch(patch: np.ndarray) -> dict[str, Any]:
    radius = 5
    background_values = [int(patch[5 + dy, 5 + dx])
                         for dy in range(-radius, radius + 1)
                         for dx in range(-radius, radius + 1)
                         if dx * dx + dy * dy < radius * radius]
    background = sum(background_values) // len(background_values)
    background_is_white = background >= 128
    contrast = np.abs(patch.astype(np.int16) - background)
    opposite = ((contrast > RADIANCE_DISTANCE_MATCH) &
                ((patch < 160) if background_is_white else (patch > 96)))

    yy, xx = np.mgrid[0:PATCH_SIZE, 0:PATCH_SIZE]
    model_x = PATCH_MODEL_LOWER + (PATCH_MODEL_UPPER - PATCH_MODEL_LOWER) * xx / PATCH_SIZE
    model_y = PATCH_MODEL_LOWER + (PATCH_MODEL_UPPER - PATCH_MODEL_LOWER) * yy / PATCH_SIZE
    inside = (((model_x - 0.5) ** 2 + (model_y - 0.5) ** 2 <= 0.25) |
              ((model_x <= 0.5) & (model_y <= 0.5)))
    predicted_opposite = ~inside
    carrier_mismatch = predicted_opposite != opposite
    carrier_loss = int(carrier_mismatch.sum())
    inside_count = int(inside.sum())
    outside_count = int((~inside).sum())
    inside_mismatch = int(carrier_mismatch[inside].sum())
    outside_mismatch = int(carrier_mismatch[~inside].sum())

    payload_support = np.zeros_like(inside)
    for row in range(6):
        for column in range(6):
            center_x, center_y = model_dot_location(column, row)
            payload_support |= ((model_x - center_x) ** 2 +
                                (model_y - center_y) ** 2 < DOT_RADIUS ** 2)
    structural_support = ~payload_support
    structural_mismatch = int(carrier_mismatch[structural_support].sum())
    structural_count = int(structural_support.sum())

    loss = carrier_loss
    payload = ["0"] * 36
    payload_improvement = 0
    for row in range(6):
        for column in range(6):
            center_x, center_y = model_dot_location(column, row)
            disk = ((model_x - center_x) ** 2 + (model_y - center_y) ** 2 < DOT_RADIUS ** 2)
            zero_loss = int(opposite[disk].sum())
            one_loss = int((~opposite[disk]).sum())
            if one_loss < zero_loss:
                improvement = one_loss - zero_loss
                loss += improvement
                payload_improvement += improvement
                bit_index = (5 - row) * 6 + (5 - column)
                payload[bit_index] = "1"
    return {
        "loss": loss,
        "carrier_loss": carrier_loss,
        "payload_improvement": payload_improvement,
        "carrier_mismatch_fraction": carrier_loss / (PATCH_SIZE * PATCH_SIZE),
        "inside_mismatch_fraction": inside_mismatch / inside_count,
        "outside_mismatch_fraction": outside_mismatch / outside_count,
        "structural_mismatch_pixels": structural_mismatch,
        "structural_support_pixels": structural_count,
        "structural_mismatch_fraction": structural_mismatch / structural_count,
        "background_average_gray": background,
        "background_polarity": "white" if background_is_white else "dark",
        "payload": "".join(payload),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path)
    parser.add_argument("trace", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    bgr = cv2.imread(str(args.image.resolve()), cv2.IMREAD_COLOR)
    if bgr is None:
        raise ValueError(f"unable to load {args.image}")
    red = bgr[:, :, 2]
    trace = read_json(args.trace.resolve())
    features = [feature for feature in trace["features"]
                if feature["accepted"] and feature["payload_extracted"]]
    if len(features) != 1:
        raise ValueError("analysis requires exactly one accepted, decoded feature")
    feature = features[0]
    carrier_model_to_input = np.array(
        feature["visual_geometry"]["carrier_projective_model_to_input"], dtype=float)
    dot_translation = np.array(((1.0, 0.0, PATCH_MODEL_LOWER),
                                (0.0, 1.0, PATCH_MODEL_LOWER),
                                (0.0, 0.0, 1.0)))
    dot_scale = np.array(((PATCH_MODEL_UPPER - PATCH_MODEL_LOWER, 0.0, 0.0),
                          (0.0, PATCH_MODEL_UPPER - PATCH_MODEL_LOWER, 0.0),
                          (0.0, 0.0, 1.0)))

    evaluations = []
    for index in range(SEARCH_STEPS):
        fraction = index / (SEARCH_STEPS - 1)
        amount = MINIMUM_AMOUNT + fraction * (MAXIMUM_AMOUNT - MINIMUM_AMOUNT)
        normalized_to_input = (carrier_model_to_input @ carrier_ambiguity_transform(amount)
                               @ dot_translation @ dot_scale)
        evaluation = evaluate_patch(sample_patch(red, normalized_to_input))
        evaluation.update({"index": index, "amount": amount,
                           "normalized_patch_to_input": normalized_to_input.tolist()})
        evaluations.append(evaluation)

    best = min(evaluations, key=lambda item: (item["loss"], abs(item["amount"])))
    payload_runs = []
    for evaluation in evaluations:
        if not payload_runs or payload_runs[-1]["payload"] != evaluation["payload"]:
            payload_runs.append({"payload": evaluation["payload"],
                                 "first_amount": evaluation["amount"],
                                 "last_amount": evaluation["amount"],
                                 "minimum_loss": evaluation["loss"]})
        else:
            payload_runs[-1]["last_amount"] = evaluation["amount"]
            payload_runs[-1]["minimum_loss"] = min(payload_runs[-1]["minimum_loss"],
                                                    evaluation["loss"])

    output = {
        "schema": "org.qyoo.detector.carrier-template-landscape",
        "schema_version": 1,
        "image": str(args.image.resolve()),
        "trace": str(args.trace.resolve()),
        "feature_index": feature["feature_index"],
        "best": best,
        "payload_runs": payload_runs,
        "evaluations": evaluations,
    }
    write_json(args.output.resolve(), output)
    print(json.dumps({"best_amount": best["amount"], "best_loss": best["loss"],
                      "best_payload": best["payload"],
                      "carrier_mismatch_fraction": best["carrier_mismatch_fraction"],
                      "payload_run_count": len(payload_runs)}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
