# Task 22 field evidence — unresolved safety incident

Exact source PNGs copied without alteration from the owner's Downloads on 2026-09-11. SHA-256, dimensions and replay results are in `replay.json`. Originals remain untouched. No detector code changed.

- `possible-wrong-accept/qyoo-baylor1.PNG`: owner-reported suspected false positive. Screenshot visibly reports raw ID 0 and all-zero bits; the displayed quadrilateral covers apparent background rather than a complete carrier. This is potential accepted-wrong evidence, **not** an established raw-frame reproduction.
- `acquisition-rejection/qyoo-baylor2-wont-scan.PNG`: a legitimate physical Qyoo with failed acquisition. Preserve separately; do not count a rejection as an accepted-wrong decode. Exact frame ground-truth identity is not assigned from appearance alone.

Both complete 1125×2436 screenshots returned zero detections in the unchanged production CLI and pinned Swift wrapper / native C ABI. The iOS application's pinned detector revision is `021becc75bd35834007aeca7a2db0f6451e8692c`; its native core provenance is CPP `3a7002fd7cc6075d9bfb19fe537585dc277a9267`. CLI defaults use carrier-template normalization, qualified fallback, bounded-camera-v1 localization. Swift replay uses the production pixel-buffer API with vendored GD. No alternate payload profile, detector tuning, image repair or invented carrier crop was used.

## Reproduce

From QyooDetector-CPP:

```
make bin/qyoo_detector
python3 tests/test_task22_field_evidence.py
```

From QyooDetector-iOS at the pinned revision:

```
swift run QyooDetectorBenchmark ../QyooDetector-CPP/tests/fixtures/task22_field_evidence/possible-wrong-accept/qyoo-baylor1.PNG 1
swift run QyooDetectorBenchmark ../QyooDetector-CPP/tests/fixtures/task22_field_evidence/acquisition-rejection/qyoo-baylor2-wont-scan.PNG 1
```

The screenshot regression checks no acceptance in the first complete screenshot and no all-zero acceptance in the acquisition example. Its passing result does **not** clear the field incident. Screenshot overlays, timing and preview transforms mean the displayed frame may differ from the actual accepted camera buffer. The accepted overlay might be stale; that is a hypothesis, not a finding.

**Mainnet/App Store readiness remains blocked.** Obtain an unmodified saved BGRA camera frame and metadata from the triggering acceptance (plus installed build identifier), reproduce through the pinned production API, then add any confirmed wrong acceptance to the known-answer wrong-accept corpus. A future fix must retain established detector safety, including accepted-wrong counts. Do not hide this by prohibiting ID 0: it is a detector safety question, independent of ID issuance policy.
