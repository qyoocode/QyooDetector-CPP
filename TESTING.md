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
while this gate fails. The audit does not change the qualified detector to
make the fixture pass. See `tests/fixtures/release-safety/manifest.json` for
reproduction provenance.
