# Qyoo Detector Core

This repository builds the production Qyoo detector as a reusable library and
also provides the historical command-line interface as a library consumer. The
portable boundary is a small C ABI; the detector implementation remains C++.

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
classes and historical recovery controls are not part of the production ABI.

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
make test-public-header test-public-api
make test-sanitizers
```

Link C or C++ clients with `-Iinclude -Llib -lqyoo_detector`. Static-library
consumers should use the C++ linker and include GD's link flags, for example
`$(pkg-config --libs gdlib)`.

## Command-line adapter

Pass a PNG or JPEG image to the detector:

```bash
bin/qyoo_detector path/to/qyoo.png
```

When a Qyoo is accepted, the command prints its raw 36-bit carrier and unsigned value:

```
Binary = 101010010011101010011001110110010101
Qyoo value = 45427039637
```

It also saves a diagnostic PNG under `output/`, with the accepted value as its filename. The image marks detected dots with green circles and clear cells with red Xs.

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

`input/45427039637.png` is retained as a known failing renderer-compatibility sample, not as a successful installation demo. Its 6x6 cells do encode `45427039637`, but the current legacy detector traces 26 features and accepts no Qyoo shape. Disposable builds of every relevant buildable detector state from `4e65f18` through `5520ced` also reject this exact file. The README command naming the file was written in October 2024; the file itself was added in April 2025, so the former “should output” claim was never supported by repository history.

The five JPEG camera photos under `input/qyoo-samples/` load successfully. With
the current full-frame detector, `IMG_5171` and `IMG_5172` extract carrier data;
`IMG_5170` is conservatively rejected because template fitting and final
sampling disagree; `IMG_5169` and `IMG_5173` fail before normalization. Their
historical payloads remain unverified, so none is a known-answer green example.

### Verbose Mode

You can add the `--v` flag to enable verbose logging. This will display debugging information such as feature detection progress and pixel data.

```bash
bin/qyoo_detector path/to/qyoo.jpg --verbose
```

Verbose mode reports image dimensions, traced features, corner/model decisions, and the final accepted-shape count. A rejected input still exits normally after printing `No Qyoo found in the image.`

Focused correctness checks are available as Make targets: `test-aspect`,
`test-contrast`, `test-background-average`, `test-resource-stress`,
`test-image-input`, `test-multiscale-localization`,
`test-carrier-template-cli`, and `test-task09b-wrong-decodes`. The recovery
workspace's frozen detector scoreboard remains the behavioral regression
authority.

### Decision-Neutral Visual Debug Export

`--visual-debug-dir` enables the raw geometry and normalized-patch export used by the recovery workspace's human visual report:

```bash
bin/qyoo_detector path/to/qyoo.jpg --visual-debug-dir /path/to/artifacts
```

The option implies structured diagnostics and writes the exact 88×88 grayscale patch used for payload sampling, plus an affine pilot when one exists. Its JSON record includes candidate contours, orientation geometry, projective correspondences and transforms, normalization outcome, and all 36 actual sample decisions. Export happens after detector decisions and does not change detection or decoding behavior.

The human renderer, one-image command, corpus command, and static report are documented at `../recovery/detector-visuals/README.md`. Run `make test-visual-diagnostics` for the focused decision-neutrality check; its disposable output is retained under the workspace recovery trash policy.

## Legacy Server-Side Usage

This project, in its original form, was used for server-side image processing
on Linux environments. The command-line adapter preserves that workflow while
the core remains independent of Objective-C and UIKit.

It is capable of handling tasks like feature detection, contrast adjustment, and image manipulation using the GD library for image input/output and the CML library for matrix operations.

## Contributing

We welcome contributions to the QyooDetector project. Please open an issue or submit a pull request if you'd like to contribute.

## License

This project is licensed under the BSD-3-Clause License. See the [LICENSE](LICENSE) file for details.
