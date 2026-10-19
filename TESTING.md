# Testing the public checkout

Install a C++11 compiler, make, pkg-config, Python 3 for Python-driven tests,
and libgd development headers/libraries. On Debian/Ubuntu:

```sh
sudo apt-get install build-essential pkg-config libgd-dev python3
make
make test-public
make test-sanitizers
```

On macOS, install libgd and pkg-config using your package manager. The Makefile
uses `pkg-config gdlib`. If metadata is unavailable it tries the compiler's
standard header/library search paths with `-lgd`; override `GD_CFLAGS` and
`GD_LIBS` explicitly for a nonstandard installation.

`test-public` uses only tracked files, including a small known-answer positive
and blank negative CLI fixture pair. Run `make test-public-smoke` for that pair. `test-public-api` uses the bundled
historical JPEG and checks expected detector output, not independently measured
camera ground truth. No accepted-wrong/rejection rate is established by that
single regression assertion. `test-public-header` only checks the C interface
and needs no linked library. Other self-contained unit targets include
`test-aspect`, `test-scale-gate`, `test-contrast`, `test-background-average`,
`test-projective-estimate`, `test-projective-sampling`, and
`test-carrier-ambiguity`.

Several historical Python harnesses and Make targets reference `../recovery/`:
resource stress, image-input, multiscale CLI, carrier-template CLI, wrong-decode,
projective CLI, diagnostics, visual diagnostics, fallback policy, and corner/edge
pair checks. Their original corpus files are not part of this repository.
They are retained as research evidence and are **not** public readiness checks.
Some targets accept fixture-path overrides in the Makefile; others require
editing a local research configuration. Do not interpret unavailable corpus
runs as passed or create substitute fixtures with guessed expected IDs.

The GitHub workflow exercises public checks, not the unavailable camera corpus.
Workflow presence alone does not establish that a run passed. A release requires
observed results on its exact revision and explicit camera ground truth.

`make clean` intentionally refuses automatic cleanup. Preserve evidence before
moving generated files into a central recoverable quarantine with original
paths, destinations, timestamps, reasons, and post-move verification.

## Release safety gate

```sh
make test-release-safety
```

This gate deliberately fails on the inspected public baseline: the included
64×64 C-generated maximum-ID symbol is accepted as 0. Rejection is acceptable;
a confidently wrong ID is not. The CI workflow enforces this gate after its
unit/API and sanitizer steps. Passing `test-public` is insufficient for release
while this gate fails. The review branch corrects the false-zero failure conservatively; the
retained baseline failure and original fixture expectations remain evidence. See `tests/fixtures/release-safety/manifest.json` for
reproduction provenance.

The safety harness accepts rejection only when the CLI exits 0, reports
`image_loaded: true` diagnostics and emits no accepted payload. Exit 1 is a
processing error, even with the text `No Qyoo found in the image.`: the CLI
uses that message for core failures, including allocation failure. The
`--self-test` reproduces this status/message case without exhausting memory.


## Compact canonical decode gate

```sh
make test-canonical-safety
```

This self-contained target is included in `test-public`, which the public
workflow runs. It executes six portable runner contract tests and16 tracked
[known-answer fixtures](tests/fixtures/canonical-safety/manifest.json). No image
rendering dependency or external corpus is needed. Predictable ignored output
is retained under `.codex-work/canonical-safety`.

The runner always fails accepted-wrong payloads and processing errors. Optional
manifest `require_decode=true` also fails rejected positives while keeping the
rejection category separate; omitted/false retains correct-or-reject behavior.
Fifteen fixtures require decoding:0/1/2 at512/256/128/64 and max at512/256/128.
Max64 remains correct-or-reject. The fresh local target produced15 correct,
1 permitted rejection,0 wrong/errors and0 required-decode rejections; hosted
CI has not been run. Native source remains unchanged from60aa253.

The broader canonical continuation added54 conditions and reused52 verified
unchanged outcomes. Correct/total: native51252/58, reduced25614/16,
reduced12816/16, reduced648/16; wrong/errors0 throughout. Different vector
coverage prevents aggregate size comparisons. Historical C-zero boundary leakage
and canonical sparse row/column search-boundary rejects remain recall holds;
see [AUDIT.md](AUDIT.md). Passing this compact gate does not establish full
canonical recall or physical-camera performance.
