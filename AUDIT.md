# Public repository audit — 18 October 2026

Inspected public default: `3a7002fd7cc6075d9bfb19fe537585dc277a9267`.
Classification: **reference detector with a portable C ABI**.
**Review branch prepared; HOLD for a newly endorsed SDK release** pending
camera/rights qualification and Apple integration checks. No main, production
project, deployment or Git history was changed.

## Critical decode: root cause and correction

**CRITICAL, corrected on this review branch:** the retained C-generated 64×64
maximum-ID PNG was accepted as zero. [Fixture manifest](tests/fixtures/release-safety/manifest.json)
retains the original SHA and expected `68719476735`; the expected ID was never
changed. Its 36 set-bit centers exist: GD's zero-diameter ellipse draws one pixel.
At this historical size SVG radius zero erases the dots instead. This is a
legacy rendering limitation, not canonical current Web geometry.

The 176×176 carrier fit and 88×88 final dot sampler both used majority support.
For the max fixture each 69-pixel sample support contained at most eight matched
foreground pixels, below the 40% decision threshold (28 pixels). Both stages
therefore agreed on an incorrect empty payload; integer conversion correctly
converted the assembled zero bits to zero. No integer overflow or bit-order
change caused this incident.

This is broader than all-one/max IDs: C-generated ID 1 also returned zero at
64, 68, 96 and 128 pixels. The app-pinned aggregate guard (one full dot's support)
rejects the original max fixture but misses sparse foreground. Requiring any
sampled foreground alone still misses C96 ID1: nearest-neighbor normalization
loses the dot from the 88px patch, while the 176px template retains eight pixels.

The correction preserves foreground evidence across both stages. Before
accepting an all-zero template payload, any foreground under the **existing**
radiance/polarity test in the complete higher-resolution grid envelope or final
sample supports causes rejection. It never substitutes another payload, changes
bit thresholds, reserves ID zero or weakens structural/ambiguity gates. A truly
empty grid remains valid. Public `include/qyoo_detector.h` is unchanged.
Diagnostics distinguish `carrier_template_empty_payload_rejected` and report
sample/template foreground counts. See [source](src/FeatureDetector.cpp).

## Same-corpus evidence

| Corpus | Correct decode | Wrong accepted | Positive rejection | Correct negative | Processing error |
|---|---:|---:|---:|---:|---:|
| Actual current Web512 (52) | 48 | 0 | 4 | 0 | 0 |
| Frozen Apple fixtures (8) | 7 | 0 | 0 | 1 | 0 |
| Historical C scales64/68/96/128/256/512 (42) | 13 | 0 | 29 | 0 | 0 |
| Held-out deterministic transforms (77) | 68 | 0 | 9 | 0 | 0 |
| Total179 | 136 | 0 | 42 | 1 | 0 |

Vectors include zero, one, max, max-minus-one, both alternating patterns and
511305600, plus 52 Web bit/boundary vectors. Transform cases include rotations,
resizing, skew/perspective, blur and polarity variations. Every fixture has an
explicit SHA and expected ID. The gate rejects nonzero process status as a
processing error, never as a safe rejection. No expected value or gate was tuned.

All seven Apple positives and the prior Web results are preserved. **Recall cost:**
five previously accepted C zero images at widths64–256 now reject. Carrier
contamination/uncertain normalization contradicts empty-grid evidence; no clean
zero ID restriction was added. A unit check accepts truly uniform zero patches
in both polarities. These results do not prove absence of wrong decodes outside
this bounded corpus or establish real-camera reliability/performance.

## iOS source impact

The release-associated app dependency is SDK `a1c3c6d203b503765243428f4fad163babcb79d9`.
Its native FeatureDetector files were compared byte-for-byte with the reproduced
aggregate guard; that source is affected by sparse false-zero acceptance. The
exact Apple-approved binary/core mapping remains unproven. SDK parity requires
this newly qualified C++ source, not merely the earlier aggregate guard. No
shipping app source was changed. The Swift adapter's orientation/ownership logic
is outside this native correction.

## Verification

Darwin arm64, Apple Clang21, installed libgd2.3.3. Previous baseline failures are
retained in workspace audit evidence, not rewritten as passes.

| Exact repository-root command | Result |
|---|---|
| `make all test-empty-payload test-public test-release-safety` | PASS final native core; three release fixtures safely reject, zero wrong; C ABI smoke positive262144 preserved. |
| `make test-empty-payload` | PASS: both polarities, uniform zero, partial foreground1–6cells, evidence absent from final patch, sparse positive; GRAY8/BGRA32 padded-stride raw ingress checks three critical PNGs and known positive, borrowed-buffer/result ownership. |
| `make test-sanitizers` | PASS final core C ABI ASan/UBSan, including repeated calls/input validation/cleanup. |
| `make test-aspect test-scale-gate test-contrast` | PASS. |
| `make test-background-average test-projective-estimate test-projective-sampling test-carrier-ambiguity` | PASS: all four targets. |
| `python3 tests/test_generated_safety.py --manifest ../safety-qualification/manifest.json --work-dir ../safety-qualification/run --report ../qualification.json` | PASS179:136correct,0wrong,42reject,1negative,0error. Manifest/report retained in workspace evidence. |
| `git diff --check` | PASS at review. |
| Linux CI / independently labeled camera holdout / physical iPhone performance | NOT RUN. |

The first multi-target continuation invocation used nonexistent `test-sentinel`;
make stopped after the first three unit targets. This invocation error was
corrected to `test-background-average`; no missing target is counted as a test pass.
An independent code review found the new unit target absent from CI's prerequisite
chain; `test-public` now includes it. No additional actionable core/ABI defect
was found in that bounded review.

## Remaining publication findings

**HIGH:** five historical camera photos lack explicit redistribution permission
and independent ground truth. Retained in `input/qyoo-samples`; do not advertise
successful photographic scanning. Historical native copyright authority also
needs owner confirmation. Qualified shipping behavior and real-camera limits
require owner-supplied labeled evidence/hardware, not guessed fixture IDs.

**MEDIUM, corrected:** complete CML Boost1.0 license restored. All301headers match
upstream `0bb27c8637dd9cdc92b2f376a9e8c08eb649920f` by blob SHA; upstream has the
same `@@COPYRIGHT@@` banners, preserved with attribution. See
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). libgd is linked externally;
codec notices depend on the consumer's actual build. The inspected GIF advisory
implementation is outside the CLI PNG/JPEG loaders and Apple subset; no reachable
Qyoo vulnerability was inferred merely from a libgd version string.

**MEDIUM, corrected documentation:** self-contained public tests/CI no longer
promise absent `../recovery` harnesses; research targets and useful history remain.
Bundled PNG/photo limitations are explicit. No independent security audit claimed.

Exposure review covered38 public-main commits/510blobs including binary blobs,
without printing credentials; no matched candidate secret found. This bounded
scan does not cover unknown formats/unreachable refs/forks or establish asset rights.
The five current JPEGs have no EXIF segment. Original exposure evidence retained.

Storage-heavy work paused below20GiB. Final bounded builds reused existing outputs
above the policy floor; approved external offload later restored more headroom.
Workspace `artifacts/public-repo-audit/STORAGE_OFFLOAD.json` records exact approved
paths, verified archive/hash and capacity. No source/history/evidence deletion.
