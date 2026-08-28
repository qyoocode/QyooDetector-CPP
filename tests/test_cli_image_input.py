#!/usr/bin/env python3
"""Focused CLI parity check for PNG, JPG, and JPEG input paths."""

from __future__ import annotations

import subprocess
import sys


EXPECTED_BITS = "0" * 36


def main() -> int:
    if len(sys.argv) != 5:
        raise SystemExit(f"usage: {sys.argv[0]} detector perfect.png perfect.jpg perfect.jpeg")
    detector = sys.argv[1]
    for image in sys.argv[2:]:
        process = subprocess.run([detector, image], text=True, capture_output=True, check=False)
        combined = process.stdout + process.stderr
        if process.returncode != 0 or f"Binary = {EXPECTED_BITS}" not in combined or "Qyoo value = 0" not in combined:
            print(f"FAIL: {image}\n{combined}", file=sys.stderr)
            return 1
    print("PASS: CLI decodes the same known answer from PNG, JPG, and JPEG")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
