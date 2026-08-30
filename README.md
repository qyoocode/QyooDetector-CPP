# QyooDetector (Command-Line Version)

This is a **command-line only** version of the QyooDetector project, designed for server-side image processing. The package has been stripped of all non-command-line code, with a focus on C++ implementations of grayscale image processing and feature detection.

## Overview

This version of **QyooDetector** is optimized for running on Linux or other command-line environments where server-side image processing is needed. It leverages the **GD** and **CML** libraries to perform various operations such as image transformation, contrast adjustments, and feature detection on grayscale images.

### Key Features

- Grayscale image processing using raw 8-bit and 32-bit image data.
- Contrast enhancement for images.
- Image flipping, saving, and matrix-based transformations.
- C++ based, making it suitable for integration in server environments.
  
This version uses **GD** to read PNG and JPEG images and write diagnostic PNGs, and **CML** for matrix transformations.

### Structure

All project files are located in the src/ folder. This version removes all dependencies on Objective-C or UIKit, making it compatible for C++-based server environments.

## Installation and Setup

### Dependencies

This version requires the following libraries:
- **GD**: For image manipulation (reading, saving, flipping).

On a typical Linux system, you can install these libraries via your package manager:

```bash
# For GD
sudo apt-get install libgd-dev
```

### Building the Project

To compile the project, navigate to the root of the repository and use the existing `Makefile` or another build system of your choice (e.g., `cmake`).

```bash
make
```

This will compile the project and output the qyoo_detector binary to the bin/ folder.

### Usage

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

This project, in its original form, was used for server-side image processing on Linux environments. The command-line only version preserves that legacy, removing all dependencies on Objective-C or UIKit, making it fully compatible with C++.

It is capable of handling tasks like feature detection, contrast adjustment, and image manipulation using the GD library for image input/output and the CML library for matrix operations.

## Contributing

We welcome contributions to the QyooDetector project. Please open an issue or submit a pull request if you'd like to contribute.

## License

This project is licensed under the BSD-3-Clause License. See the [LICENSE](LICENSE) file for details.
