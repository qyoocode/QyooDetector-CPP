# Phase 0: reproduced empty-payload wrong accepts

2026-09-12. **Current reproducible detector wrong-accept, with an automated
rejection correction; physical acceptance remains pending.** Release/Mainnet
readiness stays stopped. This supersedes the earlier non-reproducible
classification for the current detector; it does not prove the original Baylor
screenshot's exact cause.

## Evidence (FACT)

Two original 720×1280 BGRA frames from developer build 22042 contain visibly
populated Macallan carriers but return raw ID 0 through the unchanged production
C API. SHA-256 and before/after results are in
[the verification record](PHASE0_EMPTY_PAYLOAD_EVIDENCE.json).

- `7CC2FAEC-9371-498C-AFE1-0DC93353F6FF`: marked baseline720 trial expected
  `23122142570`; accepted 0 after 11.675 seconds, following 25 rejected frames.
- `3431BF75-F620-44E6-9CFC-BEB24088569F`: in-flight detector acceptance after
  the preceding Macallan B trial had ended. It has no UI presentation record;
  the native wrong acceptance still counts. Its exact true ID is not needed
  to establish that the visibly populated carrier is not zero.

Both report adjustingFocus=false, adjustingExposure=true. The focus state is
receipt-time telemetry and does not certify sharpness. The ordinary PNG preview
is color transformed: raw BGRA is the replay authority.

The first exact raw trace searches 101 projective hypotheses, all yielding
zero. Final cell matches remain below the existing 40% dot threshold. Shared
binarization permits the template and sampler to agree on a false empty grid.
Blur is a supported explanation from the visible images and sample evidence,
not a claim that every blur-induced error is covered.

## Isolated correction (FACT)

For carrier-template results that would return an empty grid, reject when the
sum of foreground samples across the 36 disjoint sample disks reaches the
existing minimum support for one dot: ceil(0.40 × 69) = 28 pixels. An individual
cell can remain below its dot threshold while the whole grid contradicts an
empty payload. The check reuses existing foreground classification and support
thresholds. It does not assign an alternative ID, alter geometry or bits, ban
ID 0, or change camera settings. Diagnostics expose the rejection and count.

This is deliberately conservative: a genuinely empty but dirty/noisy carrier
may now reject. Fully erased foreground can remain indistinguishable from an
empty carrier; this is not a universal false-accept guarantee.

## Measured verification (FACT)

| Corpus | Before | After |
|---|---|---|
| Two exact wrong-accept camera frames | 2 accepted-wrong IDs | 2 safe rejections |
| 11 other camera captures | 11 matching recorded nonzero IDs | Same 11 IDs |
| 12 valid-zero captures (same synthetic pixels) | 12 exact zero accepts | 12 exact zero accepts |
| Frozen 193 cases | 114 exact positives, 7 rejected positives, 72 rejected negatives, 0 wrong accepts | Identical per-case classifications |
| Additional 320 negatives | Established 320 rejections | 320 rejections, 0 false payloads |

The 12 repeated synthetic captures are not 12 independent scene cases. Recorded
nonzero field IDs are replay controls; they do not prove negative-scene coverage.
The seven historical wrong-decode fixtures, all-zero plus all 36 single-bit
strong-perspective controls, public C API/header, core geometry/sampling tests,
ASan/UBSan public API test, and both original screenshots pass. The new sampler
regression covers distributed partial foreground, both polarities, valid zero,
and valid sparse ID 1. No before/after phone acquisition comparison is claimed.

## Reproduce

```sh
make shared test-empty-payload test-task09b-wrong-decodes
# Private exact frames, after verifying the hashes in the evidence record:
bin/tests/test_empty_payload /path/to/7CC2FAEC/input.bgra /path/to/3431BF75/input.bgra
```

Original device exports remain local in the sibling Qyoo-iOS repository under
`.codex-work/mega-run-phase-0-20260912/accepted-frames/`. Timing records join by
frame UUID. No raw photographs or recovery archive content accompany this commit.
Unchanged source was rebuilt from commit `9951240` for paired raw comparisons;
its detector source equals production CPP `3a7002f`. Work logs, exact raw
diagnostics, and before/after scoreboards remain in this repository's matching
`.codex-work/mega-run-phase-0-20260912/` directory.

## Next gate

Propagate the corrected core to the pinned Apple package and developer app.
Then complete Baylor B baseline720 10 attempts / focus720 10 attempts and the
bounded partial/entry/exit/background/motion/defocus matrix on the corrected
build. Review every native acceptance, including unpresented results. Do not
freeze the scanner or begin Phase 1 until physical acceptance is recorded.

Parking lot: the diagnostic palette PNG writer omits grayscale index 255; some
patch exports warn. This unrelated exporter repair is deferred; exact bytes and
JSON geometry/sample diagnostics were retained.
