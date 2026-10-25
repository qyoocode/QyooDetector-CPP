# Qyoo Detector Core

Qyoo is maintained by **Pushplay LLC**.

This repository provides a classical Qyoo detector as a reusable reference library and
also provides the historical command-line interface as a library consumer. The
portable boundary is a small C ABI; the detector implementation remains C++.

This review branch contains a conservative empty-payload safety correction.
The retained 64×64 maximum-ID fixture now rejects rather than returning zero.
Qualification covers 179 explicit known-answer cases: 136 correct decodes,
42 positive rejections, one correct negative and no accepted-wrong IDs or
processing errors. This is a bounded regression result, not a camera accuracy
claim. Five small historical C zero symbols now reject. See [AUDIT.md](AUDIT.md)
for the root cause, source impact and qualification limits.

The library decodes the 36-bit visual payload; URL routing, resolution,
Manifest experiences, and ID allocation belong to other components.

## Overview

The public API accepts decoded pixel buffers. PNG/JPEG filename decoding remains
in the CLI adapter and is not part of the detector ABI. GD is an internal build
dependency and no GD type appears in the public header.

### Build outputs

- `lib/libqyoo_detector.a` — static core library
- `lib/libqyoo_detector.dylib` on macOS or `lib/libqyoo_detector.so` on Linux — shared core library
- `include/qyoo_detector.h` — public C API
- `bin/qyoo_detector` — PNG/JPEG command-line adapter

The shared library exports only the `qyoo_*` C symbols. Internal detector
classes and historical diagnostic controls are not part of the public ABI.

## Installation and Setup

### Dependencies

This version requires a C++11 compiler, `make`, `pkg-config`, and **GD**. CML is
bundled in this repository.

On a typical Linux system, you can install these libraries via your package manager:

```bash
# For GD
sudo apt-get install libgd-dev
```

### Building

To compile the project, navigate to the root of the repository and use the
provided Makefile.

```bash
make
```

This builds both libraries and the CLI. Individual targets are also available:

```bash
make library  # static library
make shared   # shared library
make bin/qyoo_detector
```

## Portable C API

The production API is declared in `include/qyoo_detector.h`. It accepts `GRAY8`,
`RGB24`, `BGR24`, `RGBA32`, or `BGRA32` rows with an explicit byte stride and
buffer length. Alpha is ignored. The frozen production localization,
normalization, sampling, and acceptance policies are selected internally; the
API does not expose recovery profiles or codecs.

```c
#include <qyoo_detector.h>

qyoo_detector_t *detector = NULL;
qyoo_result_array_t found = {0};
qyoo_image_t image = {0};

image.struct_size = sizeof(image);
image.width = width;
image.height = height;
image.stride_bytes = stride;
image.pixel_format = QYOO_PIXEL_FORMAT_RGB24;
image.pixels = pixels;
image.buffer_size = pixel_buffer_size;

if (qyoo_detector_create(&detector) == QYOO_STATUS_OK &&
    qyoo_detector_detect(detector, &image, &found) == QYOO_STATUS_OK) {
    if (found.outcome == QYOO_DETECTION_NO_QYOO) {
        /* Safe rejection: processing succeeded and no Qyoo was decoded. */
    } else {
        for (size_t i = 0; i < found.count; ++i) {
            uint64_t payload36 = found.results[i].payload;
            qyoo_bounds_t bounds = found.results[i].bounds;
            /* carrier_quadrilateral is in original-image pixel coordinates. */
        }
    }
}

qyoo_result_array_release(&found);
qyoo_detector_destroy(detector);
```

The caller owns the pixel buffer and must keep it readable only for the duration
of `qyoo_detector_detect`. The caller also owns each returned result allocation
and must release it with `qyoo_result_array_release`; never free it directly or
reuse a live result array as output to another call. Contexts are independent.
Calls on one context are serialized internally, and calls on different contexts
may run concurrently. A context must not be destroyed while a call is using it.

`QYOO_STATUS_OK` means image processing completed. The result-array outcome then
distinguishes no-Qyoo from decoded results. Invalid image metadata, allocation
failure, and processing failure are separate non-OK statuses.

For a direct API check, including malformed inputs, padded strides, repeated
calls, multiple contexts, result cleanup, and a known payload, run:

```bash
make test-public
make test-sanitizers
```

The API fixture test asserts a historical decoded value; it is a regression
check, not independent confirmation of the photographed symbol's intended ID.

Link C or C++ clients with `-Iinclude -Llib -lqyoo_detector`. Static-library
consumers should use the C++ linker and include GD's link flags, for example
`$(pkg-config --libs gdlib)`.

## Command-line adapter

Pass a PNG or JPEG image to the detector:

```bash
bin/qyoo_detector path/to/qyoo.png
```

When a Qyoo is accepted, the command prints its 36-bit binary payload and its
unsigned decimal value in the range 0 through 68,719,476,735. Binary positions
follow the oriented 6×6 grid in row-major order, most significant bit first.
The CLI also saves a diagnostic PNG under `output/`.

The default normalization fits the payload-independent carrier geometry from
the curved boundary and two straight tangents, then resolves the carrier's
one-dimensional projective ambiguity against an unknown-payload template. The
final 36 bits still come from the independent historical 88×88 deterministic
sampler; disagreement is rejected. The repaired historical path remains
available as a diagnostic control with `--normalization projective`.

Large camera images use a bounded deterministic localization policy: long-edge
1,320, then long-edge 1,600, then one native fallback. Processing stops at the
first safely accepted result. Images below a 2,400 px long edge retain native
localization. Candidate geometry is always reported in original-image
coordinates.

Carrier-template acceptance also requires payload-independent structural
agreement outside all 36 legal payload disks. Numerically unstable
projective-class choices are audited at subpixel phases and, only when that
audit is materially split, must agree with an independent 60% raster-scale
observation of the same physical carrier. A disagreement or unavailable
confirmation is rejected; payload agreement without geometric overlap is not
accepted as confirmation.

### Bundled Sample Status

`input/45427039637.png` is a retained renderer-compatibility rejection fixture,
not a successful installation demonstration. Earlier documentation's expected
successful scan was unsupported by repository history.

The five JPEG camera photos under `input/qyoo-samples/` are historical examples.
Their intended IDs have not been independently verified. The public API test
asserts the detector's historical output `511305600` for `IMG_5172.jpg`; this
checks repeatability, rather than establishing ground truth or camera accuracy.
Do not use these photos to claim a detection success rate or infer a license
for unrelated photographs.

### Verbose Mode

You can add the `--v` flag to enable verbose logging. This will display debugging information such as feature detection progress and pixel data.

```bash
bin/qyoo_detector path/to/qyoo.jpg --verbose
```

Verbose mode reports image dimensions, traced features, corner/model decisions, and the final accepted-shape count. A rejected input still exits normally after printing `No Qyoo found in the image.`

A reproducible positive smoke check and a blank rejection control are included:

```sh
make test-public-smoke
bin/qyoo_detector tests/fixtures/public-smoke/single_bit_17.png
# Binary = 000000000000000001000000000000000000
# Qyoo value = 262144
```

This synthetic single-bit example passed on the inspected public detector;
it is not a real-camera accuracy claim. Fixture provenance and hashes are in
[`tests/fixtures/public-smoke`](tests/fixtures/public-smoke).

`make test-public` runs the self-contained header, API, projective-transform,
multiscale-policy and known-answer CLI checks. [TESTING.md](TESTING.md) distinguishes these from
research targets that require external corpora. A complete camera benchmark
must count accepted-wrong IDs separately from rejection.

### Decision-Neutral Visual Debug Export

`--visual-debug-dir` enables the raw geometry and normalized-patch export used by the recovery workspace's human visual report:

```bash
bin/qyoo_detector path/to/qyoo.jpg --visual-debug-dir /path/to/artifacts
```

The option implies structured diagnostics and writes the exact 88×88 grayscale patch used for payload sampling, plus an affine pilot when one exists. Its JSON record includes candidate contours, orientation geometry, projective correspondences and transforms, normalization outcome, and all 36 actual sample decisions. Export happens after detector decisions and does not change detection or decoding behavior.

The external human report renderer and camera corpora are not distributed in
this checkout. The CLI export works independently; research validation targets
requiring those corpora are described in [TESTING.md](TESTING.md).

## Legacy Server-Side Usage

This project, in its original form, was used for server-side image processing
on Linux environments. The command-line adapter preserves that workflow while
the core remains independent of Objective-C and UIKit.

It is capable of handling tasks like feature detection, contrast adjustment, and image manipulation using the GD library for image input/output and the CML library for matrix operations.

## Contributing

We welcome contributions to the QyooDetector project. Please open an issue or submit a pull request if you'd like to contribute.

## License

Qyoo code uses [BSD-3-Clause](LICENSE). CML and libgd retain separate licenses;
see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
