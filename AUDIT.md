# Public repository audit — 18 October 2026

Inspected default branch `origin/main`:
`3a7002fd7cc6075d9bfb19fe537585dc277a9267`.
Classification: **reference implementation with a stable portable C ABI**.
Verdict: **HOLD for a newly endorsed Qyoo 4.0 SDK release**. Documentation,
license restoration, deterministic smoke/safety fixtures and CI changes are reviewable; an observed accepted-wrong decode, current-app detector alignment and
camera qualification remain open. No detector, acceptance policy,
C ABI, historical fixture, branch history, or production project was changed.

## Findings

| Severity | Finding and evidence | Status |
| --- | --- | --- |
| CRITICAL | A generated 64×64 symbol intended as 68,719,476,735 is accepted as 0. [`tests/fixtures/release-safety/manifest.json`](tests/fixtures/release-safety/manifest.json) records image/source hashes and generation command. `make test-release-safety` reproduces it. | Open, release-blocking. Failing deterministic gate added and enforced in CI. No detector changes made. |
| HIGH | Public detector snapshot predates the current app-pinned detector safety revision. In particular, current app comparison identified additional empty-payload rejection handling absent from this baseline. Public acceptance of zero is explicitly expected by the Apple fixture suite; alignment requires a deliberate product/safety decision, not a guessed 36-bit encoding change. See the pinned core provenance in the public Apple package and [`src/FeatureDetector.cpp:930`](src/FeatureDetector.cpp#L930). | Open. Preserve this qualified baseline until a separate same-corpus qualification reconciles the implementations. Do not claim shipping parity. |
| HIGH | The five historical camera photos lack an explicit photographer/redistribution grant in repository evidence: [`input/qyoo-samples`](input/qyoo-samples). Repository software licensing alone does not establish all asset rights. | Owner/legal confirmation required; all original assets retained. |
| MEDIUM | CML's complete Boost 1.0 license was absent although 301 upstream headers were distributed, including [`cml/version.h:2`](cml/version.h#L2) with a placeholder copyright banner. | Complete immutable-upstream license and exact matching source provenance restored. Original upstream banners preserved. |
| MEDIUM | README implied developer-workspace recovery harnesses were usable public regression checks; the Makefile still has unavailable external corpus defaults, e.g. [`Makefile:60`](Makefile#L60). | Public test target, CI and testing guide distinguish self-contained tests from research harnesses. Research source retained. External camera qualification remains unavailable. |
| MEDIUM | Bundled PNG rejects and historical photo IDs lack independent ground truth. The API assertion [`tests/test_public_api.cpp:186`](tests/test_public_api.cpp#L186) is a regression output oracle, not a successful independently verified photographic scan. | Misleading example/output removed; no scan success or accuracy claimed. A passing synthetic public demonstration is now included; independently labeled camera demonstration remains open. |
| LOW | GD fallback hardcoded Apple Silicon Homebrew paths and clean guidance used a developer-only recovery convention. | Compiler-standard fallback and explicit GD flag overrides documented. Automatic clean refuses changes; no files moved or deleted. |
| LOW | Historical test/harness names retain development task identifiers. | Retained as research provenance, excluded from the public smoke-test contract. No technical comments or evidence were stripped merely for appearance. |

## Compatibility and ownership

The public header and detector implementation are unchanged. API formats remain
GRAY8, RGB24, BGR24, RGBA32 and BGRA32 with explicit strides and lengths. The
caller retains input pixels for the synchronous call; result arrays require
`qyoo_result_array_release`. Per-context calls are mutex serialized in
[`src/qyoo_detector.cpp:162`](src/qyoo_detector.cpp#L162), and a context must not
be destroyed during a call. Alpha is ignored. Result geometry uses original
input pixel coordinates.

The C ABI returns the raw payload as `uint64_t` with a 36-bit count; URL/Base36
representations and resolver/Manifest behavior are outside this library. No
signature-cell or CRC-10 physical-format implementation is established by this
snapshot. No visual geometry, allocation policy or resolver behavior was changed.

All 301 CML files match upstream
[`0bb27c8637dd9cdc92b2f376a9e8c08eb649920f`](https://github.com/demianmnave/CML/tree/0bb27c8637dd9cdc92b2f376a9e8c08eb649920f/cml)
by Git blob SHA-1. Upstream includes the same `@@COPYRIGHT@@` markers and
identifies Boost 1.0. See [third-party notices](THIRD_PARTY_NOTICES.md).
libgd is system-linked and the audit host reports version 2.3.3. Distribution
notices must reflect actual downstream codec dependencies.

## Exposure review

Inspected the 38 commits reachable from `origin/main`, including 510 distinct
blobs (13 binary), via `git rev-list --objects origin/main` and
`git cat-file --batch`. Targeted searches for private-key PEM markers,
AWS access-key IDs, GitHub token formats and absolute developer home paths
found no matching content. This is a bounded review, not proof that no secret
or personal data has ever existed. Other refs and unreachable objects were not
represented as public default-branch history. No candidate secret values were
printed. The five current camera JPEGs contain no EXIF segment. Asset ownership
and image content still need owner review. No tracked checkpoints or large
binary executables were found; the largest tracked assets are the photos.

## Changes prepared

Truthful public introduction and sample limitations; self-contained `test-public`
Make target; Linux public validation workflow with read-only token permissions
and pinned checkout; complete CML and libgd license notices; portable GD fallback;
safe cleanup refusal; small synthetic known-answer/rejection smoke checks and
an intentionally failing accepted-wrong safety gate. Existing detector source
and existing tests remain unchanged.

## Verification

Commands below run from the repository root unless noted. Runtime C++ checks
used Darwin arm64, Apple Clang 21.0.0 and installed GD 2.3.3; Linux CI is
prepared but was not run.

| Command | Result |
| --- | --- |
| `make test-public-header` | PASS: C11 public header syntax check, exit 0. |
| `make -n test-public` | PASS: recipe/dependency expansion only; no compile or runtime result inferred. |
| `make clean` | PASS safety refusal: expected nonzero (make exit 2), no output directories moved/deleted. |
| `pkg-config --modversion gdlib` | PASS environment probe: 2.3.3. |
| `git diff --check` | PASS at final review. |
| `python3 ../QyooDetector-iOS/scripts/verify_sources.py --cpp .` | PASS: public header, 20 native files (documented Apple diagnostic scopes excepted) and 301 CML files match this documented core commit; 8 Apple fixture hashes/ranges also verified. |
| `make all test-public` | PASS: static/shared libraries, CLI, C header/API, projective transforms and multiscale policy. After adding smoke fixtures, `make test-public` passed again including 1 accepted-correct positive and 1 correct blank rejection; 0 accepted-wrong. Existing unused-variable compiler warnings remain. |
| `make test-aspect test-scale-gate test-contrast test-background-average test-projective-estimate test-projective-sampling test-carrier-ambiguity` | PASS: all seven unit targets. |
| `python3 tests/test_release_safety.py --self-test` | PASS: safe rejection requires exit 0, loaded-image diagnostics and no accepted values. Exit 1 always fails, including the exact CLI failed-allocation exit/message. New allocation-failure reproduction failed before the harness correction and passed after it. |
| `make test-release-safety` | FAIL: expected 68,719,476,735; accepted 0. 0 accepted-correct, 1 accepted-wrong, 0 rejections. Expected failing gate is deliberately separate from passing smoke/unit checks. |
| `bin/qyoo_detector tests/fixtures/public-smoke/single_bit_17.png` (through smoke harness) | PASS: 262144. |
| `bin/qyoo_detector ../QyooGenerate-C/.codex-work/tests/boundary.png` | FAIL compatibility: accepted 0 instead of 68,719,476,735. Identical bytes are retained in the release-safety fixture. |
| `bin/qyoo_detector ../web-symbols/DECIMAL.png` (Python subprocess loop over all 52 files) | PARTIAL compatibility: 48 accepted-correct, 0 accepted-wrong, 4 positive rejections, 0 processing errors. Rejected IDs: 32, 2048, 1073741824, 2147483648. Generated 512px vectors from current renderer source SHA-256 `60843878a6909bcb4e7afd01470633f0d8d295bd71f476daec979b142a540ef0`. Per-case output retained in the workspace audit evidence. |
| `bin/qyoo_detector ../QyooDetector-iOS/Tests/QyooDetectorTests/Fixtures/FILE` (Python loop over manifest) | PASS fixture regression: 6 accepted-correct positives, 1 correct negative, 0 accepted-wrong, 0 positive rejections; historical IMG_5172 separately returned 511305600 with intended ID unverified. |
| `bin/qyoo_detector ../QyooGenerate-PHP/.codex-work/audit-cache/111111111111111111111111111111111111.png` | FAIL positive round-trip: maximum-ID 512px PHP output rejected; 0 accepted-wrong, 1 rejection. |
| `make test-sanitizers` | NOT RUN: storage-heavy debug/sanitizer build paused below the 20 GiB free-space floor. |
| Independent known-answer camera corpus/accuracy benchmark | NOT RUN: unavailable public camera ground truth. Synthetic smoke and actual renderer round-trips above are separate measurements. |
| New GitHub CI workflow | NOT RUN: prepared locally; no push or remote execution. |

No dependency install, cleanup, history rewrite, production edit, push or
release occurred. This audit worktree began near 8.7 MiB. Small documentation, configuration,
synthetic fixture and test additions are prepared; bounded build output is
retained for reproducibility: `obj/` 1,048 KiB,
`lib/` 1,880 KiB, `bin/` 3,708 KiB and diagnostic `output/` about 208 KiB.
Worktree size grew to about 15.5 MiB (about 6.8 MiB growth). Shared volume free
space started at 20,837,516 KiB and ended near 20,849,704 KiB (about 19.9 GiB);
other workspace activity explains changes beyond this task's outputs. Only a
bounded build using installed GD was performed, estimated below 10 MiB output;
storage-heavy sanitizer/Apple builds remained paused. No disk-space reclamation
is claimed.

## Owner/legal release gates

Fix and requalify the accepted-wrong safety failure; confirm photo rights; reconcile detector safety and zero-payload policy with
the current app using independently labeled camera evidence; run sanitizers and inspect actual CI results; supply a redistributable
known-answer camera demonstration and accepted-wrong benchmark. Software BSD licensing
does not settle any trademark or patent claims about Qyoo's visual carrier.
No independent security audit is claimed.


The initial `make test-public-smoke` invocation at the product workspace root
failed because that directory has no such Make target; the corrected
repository-root invocation passed. This was an invocation error, not a detector
test failure. Workflow YAML was parsed successfully with Ruby's standard YAML
library; no remote CI run occurred.


## Dependency advisory scope checked

[Debian's CVE-2026-9672 tracker](https://security-tracker.debian.org/tracker/CVE-2026-9672)
and the [upstream PHP/libgd patch](https://github.com/php/php-src/commit/b590f0380fda26ed180874a0255f0be078434315)
identify a GIF decoder LZW defect. The Apple vendored source subset excludes
`gd_gif_in.c`; the C++ CLI selects only PNG/JPEG loaders. A reachable Qyoo
vulnerability from this advisory was not established. System libgd users should
apply their distributor's security updates; version `2.3.3` alone does not
identify the distributor patch level. This scoped check is not a blanket
security clearance for libgd or all transitive codecs.
